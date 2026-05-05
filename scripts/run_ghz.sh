#!/bin/bash

cd "$(dirname "$0")/.." || exit 1

screen -L -Logfile server_live.log -d -m -S market_server ./build/server/market_server

tail -f server_live.log &
TAIL_PID=$!

sleep 1

ghz --insecure \
  --proto proto/market/v1/market.proto \
  --call market.v1.MarketService.TradeStream \
  --async \
  -c 50 \
  -n 10000 \
  0.0.0.0:50055

kill $TAIL_PID
screen -S market_server -X quit

echo "" #new line after complete
