#!/usr/bin/env bash
# NOTE: Run ./pa3_server 1398 first!

if ! pgrep -f pa3_server >/dev/null; then
  echo "Error: pa3_server is not running. Please start the server first." >&2
  exit 1
fi

mkfifo pipe1 pipe2

./pa3_client localhost 1398 < pipe1 1>client1.stdout.txt 2>client1.stderr.txt &
client1_pid=$!
./pa3_client localhost 1398 < pipe2 1>client2.stdout.txt 2>client2.stderr.txt &
client2_pid=$!

echo "login user pass"  > pipe1
echo "login user pass"  > pipe2
echo "login AzureDiamond hunter2"  > pipe2
echo "book 42"  > pipe1
echo "book 42"  > pipe2
echo "cancel-booking 42"  > pipe1
echo "book 42"  > pipe2
echo "query 42"  > pipe2
echo "exit"  > pipe1
echo "logout"  > pipe2
echo "login user hunter2"  > pipe2
echo "login user pass"  > pipe2
echo "exit"  > pipe2
exec 3>&- 4>&-
rm pipe1 pipe2
wait "$client1_pid" "$client2_pid"