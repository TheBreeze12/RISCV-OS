#!/bin/bash
(
  sleep 2
  for i in {1..10}; do
    echo "ls"
    sleep 0.5
  done
  echo "exit"
  sleep 1
) | timeout 20 make qemu 2>&1 | tee final_test.log

if grep -q "panic" final_test.log; then
    echo "❌ 发现 panic"
    grep -B3 -A3 "panic" final_test.log | tail -10
else
    echo "✅ 测试通过！运行了 10 次 ls 命令没有 panic"
    echo "ls 输出次数："
    grep -c "FILE.*init" final_test.log
fi

