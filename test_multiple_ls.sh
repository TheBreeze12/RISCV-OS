#!/bin/bash

# 测试多次运行 ls 命令
(
  sleep 2
  echo "ls"
  sleep 1
  echo "ls"
  sleep 1
  echo "ls"
  sleep 1
  echo "ls"
  sleep 1
  echo "ls"
  sleep 1
  echo "hello"
  sleep 1
  echo "ls"
  sleep 1
  echo "exit"
  sleep 1
) | timeout 25 make qemu 2>&1 | tee multiple_ls_test.log

echo ""
echo "=== 测试结果 ==="
if grep -q "panic" multiple_ls_test.log; then
    echo "❌ 测试失败：发现 panic"
    grep -B 2 -A 2 "panic" multiple_ls_test.log
else
    echo "✅ 测试通过：没有发现 panic"
    echo "成功运行的 ls 命令次数："
    grep -c "FILE.*init" multiple_ls_test.log
fi

