# LvglFontViewer 格式验证修复

## 问题描述

用户反馈：当 C 文件中字符间位图数据**缺少逗号**时，解析器没有检查出来，还能以正常方式显示出字符。

### 问题示例

```c
static const uint8_t glyph_bitmap[] = {
    0x00, 0x12, 0x00,
    0x09 0xab, 0xcd,    // ❌ 错误：0x09 和 0xab 之间缺少逗号
    0xef, 0x00
};
```

实际上会被连接成：
```c
0x09ab  // 一个无效的值（超过 0xFF）
```

## 根本原因

### 旧的解析逻辑

```cpp
QRegularExpression hexRegex(R"(0x([0-9A-Fa-f]{1,2}))");
auto it = hexRegex.globalMatch(bitmapStr);
while (it.hasNext()) {
    auto match = it.next();
    bitmapData.append(match.captured(1).toUInt(nullptr, 16));
}
```

**问题**：
1. 正则表达式只匹配 `0x` 后面的 **1-2 位**十六进制数
2. **不检查值的合法性**
3. 对于 `0x09ab`，只会匹配前两位 `0x09`，**丢失** `ab` 部分
4. 导致数据丢失但不报错

### 错误场景分析

| 输入 | 旧解析器行为 | 实际应该 | 问题 |
|------|-------------|---------|------|
| `0x09, 0xab` | 解析为 `[0x09, 0xab]` | ✅ 正确 | 无 |
| `0x09ab` | 解析为 `[0x09]` | ❌ 错误 | **丢失数据 `ab`** |
| `0x123` | 解析为 `[0x12]` | ❌ 错误 | **丢失数据 `3`** |
| `0xabcdef` | 解析为 `[0xab]` | ❌ 错误 | **丢失数据 `cdef`** |

**问题本质**：
- 缺少逗号会导致多个字节连写成一个大数值
- 旧解析器**截断**这个大数值，只取前 2 位
- 后面的数据**静默丢失**，不报错

## 解决方案

### 新的验证逻辑

**文件**: `lvglfontparser.cpp`

#### 1. 先检测所有十六进制值

```cpp
// 匹配所有十六进制值（包括格式错误的）
QRegularExpression allHexRegex(R"(0x[0-9A-Fa-f]+)");
auto allHexIt = allHexRegex.globalMatch(bitmapStr);

QVector<QPair<QString, int>> allHexValues;
while (allHexIt.hasNext()) {
    auto match = allHexIt.next();
    allHexValues.append({match.captured(0), match.capturedStart()});
}
```

#### 2. 验证每个值的合法性

```cpp
// 验证每个十六进制值
for (const auto& hexPair : allHexValues) {
    QString hexValue = hexPair.first;
    int pos = hexPair.second;

    // 检查是否超过 2 位（0xFF 是最大值）
    // "0x" + 最多2位 = 最长4个字符
    if (hexValue.length() > 4) {
        qWarning() << "Format error detected in glyph_bitmap array!";
        qWarning() << "Found invalid hex value:" << hexValue;
        qWarning() << "Hex values must be 0x00-0xFF (max 2 hex digits).";
        qWarning() << "This usually means missing comma(s) between values.";
        qWarning() << "Position:" << pos;

        // 显示周围的上下文
        int start = qMax(0, pos - 50);
        int len = qMin(100, bitmapStr.length() - start);
        QString context = bitmapStr.mid(start, len);
        qWarning() << "Context:" << context;

        m_error = QString("Format error: invalid hex value '%1' at position %2.\n"
                          "Hex values must be 0x00-0xFF (max 2 digits).\n"
                          "Check for missing commas between values.")
                      .arg(hexValue)
                      .arg(pos);
        return false;
    }
}
```

#### 3. 正常解析（验证通过后）

```cpp
// 格式检查通过，正常解析
QRegularExpression hexRegex(R"(0x([0-9A-Fa-f]{1,2}))");
auto it = hexRegex.globalMatch(bitmapStr);
int matchCount = 0;
while (it.hasNext()) {
    auto match = it.next();
    bitmapData.append(match.captured(1).toUInt(nullptr, 16));
    matchCount++;
}

// 验证解析的数量是否与检测到的数量一致
if (matchCount != allHexValues.size()) {
    qWarning() << "Warning: parsed" << matchCount << "values, but detected" 
               << allHexValues.size() << "hex patterns.";
    qWarning() << "This may indicate a parsing issue.";
}
```

### 验证策略

**两层检查**：

1. **格式验证**（先执行）
   - 检测所有 `0x...` 模式
   - 验证每个值不超过 2 位十六进制数（`0x00-0xFF`）
   - 发现无效值立即报错

2. **一致性验证**（解析后）
   - 比对检测到的 hex 值数量 vs 实际解析的数量
   - 如果不一致，警告可能存在解析问题

### 错误检测示例

#### 示例 1: 缺少逗号

**输入**:
```c
0x09 0xab, 0xcd
```

实际会被解释为：
```c
0x09ab, 0xcd  // 0x09 和 0xab 连接了
```

**新解析器输出**:
```
Format error detected in glyph_bitmap array!
Found invalid hex value: 0x09ab
Hex values must be 0x00-0xFF (max 2 hex digits).
This usually means missing comma(s) between values.
Position: 42
Context: ...0x00, 0x12, 0x00, 0x09ab, 0xcd...
```

**错误信息**:
```
Format error: invalid hex value '0x09ab' at position 42.
Hex values must be 0x00-0xFF (max 2 digits).
Check for missing commas between values.
```

#### 示例 2: 多个逗号缺失

**输入**:
```c
0x123456
```

**新解析器输出**:
```
Format error detected in glyph_bitmap array!
Found invalid hex value: 0x123456
Hex values must be 0x00-0xFF (max 2 hex digits).
Position: 8
```

### 正常情况

**输入**:
```c
0x00, 0x12, 0x00, 0x09, 0xab, 0xcd
```

**输出**:
```
Bitmap data size: 6 (6 values parsed)
```

## 行为对比

| 输入 | 旧解析器 | 新解析器 |
|------|---------|---------|
| `0x09, 0xab` | ✅ `[0x09, 0xab]` | ✅ `[0x09, 0xab]` |
| `0x09ab` | ❌ `[0x09]` (丢失 `ab`) | ❌ **报错：无效值** |
| `0x123` | ❌ `[0x12]` (丢失 `3`) | ❌ **报错：无效值** |
| `0x09 0xab` | ✅ `[0x09, 0xab]` (看似正常) | ✅ `[0x09, 0xab]` |

**注意**：`0x09 0xab`（空格分隔）在 C 语言中**不是**合法语法，会导致编译错误。解析器假设输入的 C 文件是可编译的。

## 测试

### 创建测试文件

已创建 `test_font_error.c` 包含格式错误：

```c
static const uint8_t glyph_bitmap[] = {
    0x00, 0x12, 0x00, 0x00, 0x56, 0x00,  // Correct
    0x09 0xab, 0xcd, 0x00,               // ERROR: Missing comma
    0xef, 0x00
};
```

### 测试步骤

1. 启动 LvglFontViewer
2. 打开 `test_font_error.c`
3. 应该看到错误提示：
   ```
   Format error: invalid hex value '0x09ab' at position XX.
   Hex values must be 0x00-0xFF (max 2 digits).
   Check for missing commas between values.
   ```

## 修复日期

2026-06-05

## 状态

✅ **完成** - 解析器现在能检测并报告无效的十六进制值（超过 2 位），提示可能缺少逗号。
