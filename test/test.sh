#!/bin/sh

UBR_TEST_N_PORTS=${UBR_TEST_N_PORTS:-4}

alias untagged="sed 's/$/-u/g'"
alias tagged="sed 's/$/-t/g'"

all() {
    seq 0 $UBR_TEST_N_PORTS
}

all_except() {
    all | grep -ve "^${1}\$"
}

trap 'exit 1' INT HUP QUIT TERM ALRM USR1
trap 'cleanup' EXIT

tcpdumps=

cleanup() {
    echo cleanup

    for iface in $(cat /proc/net/dev | awk -F: '/ubr-test/ { print($1); }'); do
	ip link del dev $iface 2>/dev/null
    done
}

setup() {
    echo setup

    truncate -s 0 p0.pcapng p0.expected p0.observed
    ip link add dev ubr-test-p0 type ubr
    ip link set dev ubr-test-p0 up
    tcpdump -qni ubr-test-p0 -Q in -w p0.pcapng 2>/dev/null &
    tcpdumps="$tcpdumps $!"

    for i in $(all_except 0); do
	truncate -s 0 p$i.pcapng p$i.expected p$i.observed
	ip link add dev ubr-test-p$i type veth peer name ubr-test-b$i
	ip link set dev ubr-test-b$i master ubr-test-p0
	ip link set dev ubr-test-b$i up
	ip link set dev ubr-test-p$i up
	tcpdump -qni ubr-test-p$i -Q in -w p$i.pcapng 2>/dev/null &
	tcpdumps="$tcpdumps $!"
    done
}

seqno=0

parse_expect() {
    local vid=$1
    shift

    while [ "$#" -gt 0 ]; do
	local port=${1%-*}
	local tag=${1#*-}

	printf "seqno:${seqno}" >>p$port.expected

	if [ "$tag" = "t" ]; then
	    printf " vlan:${vid}" >>p$port.expected
	fi

	echo >>p$port.expected
    	shift
    done
}

inject() {
    local desc="$1"
    local iif=p$2
    local da=$3
    local sa=$4
    local vid=${5%-*}
    local tag=${5#*-}
    shift 5

    parse_expect $vid $@

    printf "%4u: ->%-3s: %s\n" $seqno "$iif" "$desc"

    # Simple LLDP (88cc) frame which is easy to parse with tcpdump
    printf "\x$(printf $da | sed -e 's/:/\\x/g')"  >  ${seqno}.pkt
    printf "\x$(printf $sa | sed -e 's/:/\\x/g')"  >> ${seqno}.pkt
    if [ "${tag}" = "t" ]; then
	printf "\x81\x00" >> ${seqno}.pkt
	printf "\x$(printf %02x $(($vid >> 8)))"   >> ${seqno}.pkt
	printf "\x$(printf %02x $(($vid & 0xff)))" >> ${seqno}.pkt
    fi

    printf "\x88\xcc"                              >> ${seqno}.pkt
    # LLDP System name TLV payload, with two zero bytes as end marker
    printf "\x0a\x14%-20s\x00\x00" $seqno          >> ${seqno}.pkt

    # Send frame, on error this pkt file can be re-used to debug
    socat gopen:${seqno}.pkt interface:ubr-test-$iif

    seqno=$(($seqno + 1))
}

verify() {
    tcpdump -enr p$1.pcapng 2>/dev/null  | awk '{
    	for (i = 1; i <= NF; i++) {
	    if ($i == "vlan") {
	    	i++;
		printf("seqno:%s vlan:%u\n", $NF, $i);
		next;
	    }
	}
	
	printf("seqno:%s\n", $NF)
    }' >p$1.observed

    if ! diff -u p$1.expected p$1.observed; then
	echo p$1 sequence FAIL
	return 1
    else
	echo p$1 sequence OK
    fi
}

report() {
    for pid in $tcpdumps; do kill $pid; done
    for pid in $tcpdumps; do wait $pid; done

    local code=0
    for i in $(all); do
	if ! verify $i; then
	    code=$(($code + 1))
	fi
    done

    exit $code
}

setup
sleep 1

echo inject

inject "broadcast: host" 0 \
       "ff:ff:ff:ff:ff:ff" "02:ed:00:00:00:01" 0-u \
       $(all_except 0 | untagged)
inject "broadcast: port" 1 \
       "ff:ff:ff:ff:ff:ff" "02:ed:00:00:01:01" 0-u \
       $(all_except 1 | untagged)

inject "unknown uc: host" 0 \
       "02:00:de:ad:00:01" "02:ed:00:00:00:01" 0-u \
       $(all_except 0 | untagged)
inject "unknown uc: port" 1 \
       "02:00:de:ad:00:01" "02:ed:00:00:01:01" 0-u \
       $(all_except 1 | untagged)

inject "unknown mc: host" 0 \
       "03:00:de:ad:00:01" "02:ed:00:00:00:01" 0-u \
       $(all_except 0 | untagged)
inject "unknown mc: port" 1 \
       "03:00:de:ad:00:01" "02:ed:00:00:01:01" 0-u \
       $(all_except 1 | untagged)

inject "learning: host insert" 0 \
       "ff:ff:ff:ff:ff:ff" "02:ed:00:00:00:01" 0-u \
       $(all_except 0 | untagged)
inject "learning: port reply/insert" 1 \
       "02:ed:00:00:00:01" "02:ed:00:00:01:01" 0-u \
       0-u
inject "learning: port reply" 0 \
       "02:ed:00:00:01:01" "02:ed:00:00:00:01" 0-u \
       1-u

inject "learning: host move" 0 \
       "ff:ff:ff:ff:ff:ff" "02:ed:00:00:01:01" 0-u \
       $(all_except 0 | untagged)
inject "learning: port move" 1 \
       "ff:ff:ff:ff:ff:ff" "02:ed:00:00:00:01" 0-u \
       $(all_except 1 | untagged)
inject "learning: host move ok" 2 \
       "02:ed:00:00:01:01" "02:ed:00:00:02:01" 0-u \
       0-u
inject "learning: port move ok" 2 \
       "02:ed:00:00:00:01" "02:ed:00:00:02:01" 0-u \
       1-u

inject "vlan transparency: host" 0 \
       "ff:ff:ff:ff:ff:ff" "02:ed:00:00:00:01" 1-t \
       $(all_except 0 | tagged)
inject "vlan transparency: port" 1 \
       "ff:ff:ff:ff:ff:ff" "02:ed:00:00:01:01" 1-t \
       $(all_except 1 | tagged)


sleep 1
report
