// 头文件
#include "crc16_modbus.h"

// 函数定义

//
// CRC-16-Modbus 原理简介：
//
// CRC（循环冗余校验）把数据看作一个大二进制数，用多项式除法求余数。
// Modbus 协议使用的参数如下：
//   - 多项式: 0x8005（反映射后为 0xA001，即代码中用的值）
//   - 初始值: 0xFFFF
//   - 结果异或: 0x0000（Modbus 标准为 0x0000，而 CRC32 为 0xFFFFFFFF）
//   - 大端序传输
//
// 运算过程（逐字节）：
//   1. crc = 0xFFFF（初始值）
//   2. 对每个字节：crc = crc XOR 字节
//   3. 对 crc 的低 8 位逐位判断（共 8 次）：
//      - 如果最低位 LSB = 1：右移 1 位，再 XOR 多项式 0xA001
//      - 如果最低位 LSB = 0：仅右移 1 位
//   4. 所有字节处理完后的 crc 即为校验值
//
// 例：赛题中的重启命令帧
//   ASCII:  A5B600010101000215ABB6A5
//   二进制: A5 B6 00 01 01 01 00 02
//           帧头  设备ID 类 命令  长 版
//
//   计算 CRC 的范围 = [A5 B6 00 01 01 01 00 02]（起始标志~内容，不含 CRC 和结束标志）
//   对这 8 个字节运行 CRC-16-Modbus，结果 = 0x15AB
//   帧中最后 4 个 ASCII 字符 "15AB" 即 CRC 值的大端十六进制表示
//

//
// Comment  :       CRC-16 Modbus 校验计算
// Parameter:       data - 待校验的数据缓冲区
//                  len  - 数据长度（字节数）
// Return   :       16 位 CRC 校验值
//
// 调用示例：
//   uint8_t frame[] = {0xA5, 0xB6, 0x00, 0x01, 0x01, 0x01, 0x00, 0x02};
//   uint16_t crc = crc16_modbus(frame, 8);  // 结果 = 0x15AB
//
uint16_t crc16_modbus(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFF;
    uint32_t i, j;

    for (i = 0; i < len; i++)
    {
        crc ^= data[i];                     // 异或当前字节到 CRC 低 8 位
        for (j = 0; j < 8; j++)             // 逐位处理
        {
            if (crc & 0x0001)               // 如果 LSB=1
            {
                crc = (crc >> 1) ^ 0xA001;  // 右移后异或多项式
            }
            else                            // 如果 LSB=0
            {
                crc >>= 1;                  // 仅右移
            }
        }
    }
    return crc;                             // 无需额外异或（Modbus 标准如此）
}
