#ifndef __EIENNET_IO_HPP__
#define __EIENNET_IO_HPP__

/** \brief IO模型 */
namespace io
{
/** \brief IO类型 */
enum IoType
{
    ioNone,
    ioAccept,
    ioConnect,
    ioRecv,
    ioSend,
    ioRecvFrom,
    ioSendTo,
    ioTimer
};

/** \brief 取消类型 */
enum CancelType
{
    cancelNone, //!< 不取消
    cancelProactive, //!< 主动取消
    cancelTimeout //!< 超时自动取消
};



} // namespace io

#endif // __EIENNET_IO_HPP__
