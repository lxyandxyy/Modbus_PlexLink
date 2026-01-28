#include "interfaces.h"

namespace ModbusPlexLink {

// ICollector的虚拟析构函数实现
// 这个文件主要是为Qt的MOC系统提供必要的符号
// 因为ICollector和IServer使用了Q_OBJECT宏，MOC需要生成元对象代码

// 注意：信号不需要实现，Qt会自动生成
// 但是我们需要这个.cpp文件让MOC能够编译生成的moc文件

} // namespace ModbusPlexLink

