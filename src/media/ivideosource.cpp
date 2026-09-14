#include "ivideosource.h"

QString IVideoSource::stateText(VideoSourceState state)
{
    switch (state) {
    case VideoSourceState::Closed:
        return QStringLiteral("未打开视频");
    case VideoSourceState::Opening:
        return QStringLiteral("正在打开视频");
    case VideoSourceState::Playing:
        return QStringLiteral("正在播放视频");
    case VideoSourceState::Ended:
        return QStringLiteral("视频播放结束");
    case VideoSourceState::Interrupted:
        return QStringLiteral("视频输入中断");
    case VideoSourceState::Reconnecting:
        return QStringLiteral("正在重连视频输入");
    case VideoSourceState::Error:
        return QStringLiteral("视频输入错误");
    case VideoSourceState::Stopped:
        return QStringLiteral("视频已停止");
    }
    return QStringLiteral("未知视频状态");
}

bool IVideoSource::shouldKeepPolling(VideoSourceState state)
{
    return state == VideoSourceState::Playing
            || state == VideoSourceState::Interrupted
            || state == VideoSourceState::Reconnecting;
}
