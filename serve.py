"""
开发用 HTTP 服务器：
  1. 对所有响应添加 Cache-Control: no-store 头，彻底禁止浏览器缓存
  2. 忽略 If-Modified-Since 请求头，永远返回 200（不返回 304）
解决 Edge 缓存旧版 wasm/js 的问题。
用法: python serve.py [端口]
"""
import http.server
import socketserver
import sys
import os

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 3000


class NoCacheHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        # 彻底禁止缓存
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        super().end_headers()

    def send_head(self):
        # 忽略 If-Modified-Since，强制返回 200 而不是 304
        for header in ("If-Modified-Since", "If-None-Match"):
            if header in self.headers:
                del self.headers[header]
        return super().send_head()


if __name__ == "__main__":
    with socketserver.TCPServer(("", PORT), NoCacheHandler) as httpd:
        httpd.serve_forever()
