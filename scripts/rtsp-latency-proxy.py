# -*- coding: utf-8 -*-
"""RTSP 延迟/抖动/断连测试代理（仅本机开发测试用）。

用途：在拉流客户端与本机 RTSP 服务器之间插入可控网络劣化，
验证 RtspSource 在延时、抖动与连接中断下的行为。
拓扑：控制台 ffmpeg -> 127.0.0.1:<listen-port>（本代理） -> 127.0.0.1:<target-port>（MediaMTX）

示例：
  python scripts/rtsp-latency-proxy.py --listen-port 8600 --latency-ms 100 --jitter-ms 50
  python scripts/rtsp-latency-proxy.py --listen-port 8600 --drop-after-seconds 20

约束：只处理单 TCP 连接字节流（-rtsp_transport tcp 的 interleaved RTP 走同一连接）。
不监听 8554，不访问网络外部地址，默认只绑定 127.0.0.1。
"""

import argparse
import asyncio
import random
import sys
import time

CHUNK = 65536


class Pump:
    """单向管道：读到数据后按 due=到达时刻+延迟+抖动 定时写出，保持顺序与吞吐。"""

    def __init__(self, reader, writer, latency_s, jitter_s, tag, on_eof):
        self.reader = reader
        self.writer = writer
        self.latency_s = latency_s
        self.jitter_s = jitter_s
        self.tag = tag
        self.on_eof = on_eof
        self.queue = asyncio.Queue()
        self.prev_due = 0.0

    async def read_loop(self):
        while True:
            data = await self.reader.read(CHUNK)
            if not data:
                break
            jitter = random.uniform(0, self.jitter_s) if self.jitter_s > 0 else 0.0
            self.queue.put_nowait((time.monotonic() + self.latency_s + jitter, data))
        self.queue.put_nowait(None)

    async def write_loop(self):
        while True:
            item = await self.queue.get()
            if item is None:
                break
            due, data = item
            due = max(due, self.prev_due)  # 单 TCP 流必须保序
            self.prev_due = due
            wait = due - time.monotonic()
            if wait > 0:
                await asyncio.sleep(wait)
            self.writer.write(data)
            await self.writer.drain()
        self.on_eof(self.tag)


async def handle(client_reader, client_writer, args, stats, stop_event):
    peer = client_writer.get_extra_info("peername")
    started = time.monotonic()
    try:
        upstream_reader, upstream_writer = await asyncio.wait_for(
            asyncio.open_connection(args.target_host, args.target_port),
            timeout=10,
        )
    except Exception as exc:  # 目标不可达时保持客户端存活并记录
        print(f"{ts()} accept client={peer} upstream connect FAILED: {exc}", flush=True)
        client_writer.close()
        return

    stats["connections"] += 1
    conn_id = stats["connections"]
    print(f"{ts()} conn#{conn_id} open client={peer}", flush=True)

    closed = {"done": 0}

    def on_eof(tag):
        closed["done"] += 1
        # 任一方向结束后关闭整条连接，触发客户端重连
        for w in (client_writer, upstream_writer):
            try:
                w.close()
            except Exception:
                pass
        if closed["done"] >= 1 and not stop_event.is_set():
            print(f"{ts()} conn#{conn_id} closed by {tag}", flush=True)

    pumps = [
        Pump(client_reader, upstream_writer, args.latency_ms / 1000.0,
             args.jitter_ms / 1000.0, "c->u", on_eof),
        Pump(upstream_reader, client_writer, args.latency_ms / 1000.0,
             args.jitter_ms / 1000.0, "u->c", on_eof),
    ]
    tasks = [asyncio.create_task(p.read_loop()) for p in pumps] + \
            [asyncio.create_task(p.write_loop()) for p in pumps]

    drop_task = None
    if args.drop_after_seconds > 0:
        async def dropper():
            await asyncio.sleep(args.drop_after_seconds)
            if stop_event.is_set():
                return
            stats["drops"] += 1
            print(f"{ts()} conn#{conn_id} DROP after {args.drop_after_seconds}s (simulated network cut)",
                  flush=True)
            for w in (client_writer, upstream_writer):
                try:
                    w.close()
                except Exception:
                    pass

        drop_task = asyncio.create_task(dropper())

    await asyncio.gather(*tasks, return_exceptions=True)
    if drop_task is not None:
        drop_task.cancel()
    print(f"{ts()} conn#{conn_id} done duration={time.monotonic() - started:.1f}s", flush=True)


def ts():
    return time.strftime("%Y-%m-%d %H:%M:%S")


async def main():
    parser = argparse.ArgumentParser(description="RTSP latency/jitter/drop TCP proxy (loopback only)")
    parser.add_argument("--listen-host", default="127.0.0.1")
    parser.add_argument("--listen-port", type=int, required=True)
    parser.add_argument("--target-host", default="127.0.0.1")
    parser.add_argument("--target-port", type=int, required=True)
    parser.add_argument("--latency-ms", type=float, default=0.0)
    parser.add_argument("--jitter-ms", type=float, default=0.0)
    parser.add_argument("--drop-after-seconds", type=float, default=0.0,
                        help="每个连接建立后多少秒强制断开；0 表示不掐断")
    args = parser.parse_args()

    if args.drop_after_seconds > 0 and args.drop_after_seconds < 1:
        print("drop-after-seconds must be >= 1", file=sys.stderr)
        return 2

    stats = {"connections": 0, "drops": 0}
    stop_event = asyncio.Event()

    async def client_connected(reader, writer):
        await handle(reader, writer, args, stats, stop_event)

    server = await asyncio.start_server(client_connected, args.listen_host, args.listen_port)
    addr = server.sockets[0].getsockname()
    print(f"{ts()} proxy listening {addr[0]}:{addr[1]} -> {args.target_host}:{args.target_port} "
          f"latency={args.latency_ms}ms jitter={args.jitter_ms}ms "
          f"drop_after={args.drop_after_seconds}s", flush=True)
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    try:
        sys.exit(asyncio.run(main()))
    except KeyboardInterrupt:
        pass
