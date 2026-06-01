#!/usr/bin/env bash
set -euo pipefail

# NOTE: Run ./pa3_server 1398 first!

if ! pgrep -f pa3_server >/dev/null; then
  echo "Error: pa3_server is not running. Please start the server first." >&2
  exit 1
fi

rm -f pipe1 pipe2
mkfifo pipe1 pipe2
cleanup() {
  exec 3>&- 4>&- 2>/dev/null || true
  rm -f pipe1 pipe2
}
trap cleanup EXIT

./pa3_client localhost 1398 < pipe1 1>client1.stdout.txt 2>client1.stderr.txt &
client1_pid=$!
./pa3_client localhost 1398 < pipe2 1>client2.stdout.txt 2>client2.stderr.txt &
client2_pid=$!

exec 3>pipe1
exec 4>pipe2

send_client1() {
  printf "%s\n" "$1" >&3
  sleep 0.2
}

send_client2() {
  printf "%s\n" "$1" >&4
  sleep 0.2
}

send_client1 "login user pass"
send_client2 "login user pass"
send_client2 "login AzureDiamond hunter2"
send_client1 "book 42"
send_client2 "book 42"
send_client1 "cancel-booking 42"
send_client2 "book 42"
send_client2 "query 42"
send_client1 "exit"
send_client2 "logout"
send_client2 "login user hunter2"
send_client2 "login user pass"
send_client2 "exit"

wait "$client1_pid" "$client2_pid"
