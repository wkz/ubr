#!/bin/sh

UBR_TEST_N_PORTS=${UBR_TEST_N_PORTS:-4}

alias all="seq 0 $UBR_TEST_N_PORTS"

exclude() {
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

    for i in $(exclude 0); do
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

inject() {
    local desc="$1"
    local iif=p$2
    local da=$3
    local sa=$4
    local tag=$5
    shift 5

    local oifs=p$1
    echo $seqno >>p$1.expected
    shift

    while [ "$#" -gt 0 ]; do
    	oifs=$oifs,p$1
	echo $seqno >>p$1.expected
    	shift
    done

    printf "%4u: %-3s ->  %-20s (%s)\n" $seqno "$iif" "$oifs" "$desc"

    printf "\\x$(printf $da | sed -e 's/:/\\x/g')\\x$(printf $sa | sed -e 's/:/\\x/g')\x00\x40%-20s" $seqno | socat stdin interface:ubr-test-$iif

    seqno=$(($seqno + 1))
}

verify() {
    tcpdump -r p$1.pcapng -nnA 2>/dev/null  | grep -v length | awk '{ print($1); }' >p$1.observed

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
       "ff:ff:ff:ff:ff:ff" "02:ed:00:00:00:01" untagged \
       $(exclude 0)
inject "broadcast: port" 1 \
       "ff:ff:ff:ff:ff:ff" "02:ed:00:00:01:01" untagged \
       $(exclude 1)

inject "unknown uc: host" 0 \
       "02:00:de:ad:00:01" "02:ed:00:00:00:01" untagged \
       $(exclude 0)
inject "unknown uc: port" 1 \
       "02:00:de:ad:00:01" "02:ed:00:00:01:01" untagged \
       $(exclude 1)

inject "unknown mc: host" 0 \
       "03:00:de:ad:00:01" "02:ed:00:00:00:01" untagged \
       $(exclude 0)
inject "unknown mc: port" 1 \
       "03:00:de:ad:00:01" "02:ed:00:00:01:01" untagged \
       $(exclude 1)

inject "learning: host insert" 0 \
       "ff:ff:ff:ff:ff:ff" "02:ed:00:00:00:01" untagged \
       $(exclude 0)
inject "learning: port reply/insert" 1 \
       "02:ed:00:00:00:01" "02:ed:00:00:01:01" untagged \
       0
inject "learning: port reply" 0 \
       "02:ed:00:00:01:01" "02:ed:00:00:00:01" untagged \
       1

inject "learning: host move" 0 \
       "ff:ff:ff:ff:ff:ff" "02:ed:00:00:01:01" untagged \
       $(exclude 0)
inject "learning: port move" 1 \
       "ff:ff:ff:ff:ff:ff" "02:ed:00:00:00:01" untagged \
       $(exclude 1)
inject "learning: host move ok" 2 \
       "02:ed:00:00:01:01" "02:ed:00:00:02:01" untagged \
       0
inject "learning: port move ok" 2 \
       "02:ed:00:00:00:01" "02:ed:00:00:02:01" untagged \
       1


sleep 1
report
