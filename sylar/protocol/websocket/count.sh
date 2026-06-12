
#!/bin/bash

# 递归统计指定类型文件的代码行数（不计空行）

# 检查是否传入目录参数，如果没有则使用当前目录
TARGET_DIR="${1:-.}"

# 检查目录是否存在
if [ ! -d "$TARGET_DIR" ]; then
    echo "错误: 目录 '$TARGET_DIR' 不存在"
    exit 1
fi

echo "正在统计目录: $TARGET_DIR"
echo "包含文件类型: .cc .c .h .hpp .cpp"
echo "----------------------------------------"

# 统计总行数
TOTAL_LINES=0

# 遍历每种文件类型并统计
for ext in cc c h hpp cpp; do
    # 查找文件并统计非空行
    LINES=$(find "$TARGET_DIR" -type f -name "*.$ext" -exec grep -v '^[[:space:]]*$' {} \; 2>/dev/null | wc -l)
    
    if [ "$LINES" -gt 0 ]; then
        printf "%-8s : %8d 行\n" "*.$ext" "$LINES"
        TOTAL_LINES=$((TOTAL_LINES + LINES))
    fi
done

echo "----------------------------------------"
printf "%-8s : %8d 行\n" "总计" "$TOTAL_LINES"

# 可选：显示文件数量
echo ""
echo "文件数量统计:"
for ext in cc c h hpp cpp; do
    COUNT=$(find "$TARGET_DIR" -type f -name "*.$ext" 2>/dev/null | wc -l)
    if [ "$COUNT" -gt 0 ]; then
        printf "%-8s : %8d 个文件\n" "*.$ext" "$COUNT"
    fi
done
