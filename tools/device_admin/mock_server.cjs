// 本地 mock 设备：按 DeviceAdminService 的真实语义实现同一套 HTTP 接口，
// 用于在没有硬件时联调 tools/device_admin/index.html。不参与固件构建。
const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const { URL } = require('node:url');

const PAGE = path.join(__dirname, 'index.html');
const TOKEN = 'mock-token-1234';
const SETTINGS = { name: 'OpenBread N16R8', sleep: 60, timeout: 1800 };

const dir = (n) => ({ name: n, dir: true, size: 0 });
const file = (n, s) => ({ name: n, dir: false, size: s });
function buildTree() {
  return {
    '/': [dir('Font'), dir('Music'), dir('Book'), dir('Gif'), dir('models'), file('settings.json', 412), file('日志.txt', 4196)],
    '/Font': [file('font-14-ui.bin', 62092), file('font-20-ui.bin', 118440), file('font-24-ui.bin', 131072), file('font-full.bin', 3355443)],
    '/Music': [file('夜曲.mp3', 8410624), file('晴天.mp3', 9123456), file('稻香.flac', 24681728)],
    '/Book': [file('三体.txt', 1887436), file('我的世界.json', 412)],
    '/Gif': [file('loading.gif', 98304)],
    '/models': []
  };
}
let TREE = buildTree();
let ota = { state: 0, error: 0, progress: 0, version: '' };

function json(res, code, body) {
  res.writeHead(code, { 'Content-Type': 'application/json' });
  res.end(typeof body === 'string' ? body : JSON.stringify(body));
}
function text(res, code, body) {
  res.writeHead(code, { 'Content-Type': 'text/plain; charset=utf-8' });
  res.end(body);
}

const server = http.createServer((req, res) => {
  const url = new URL(req.url, 'http://127.0.0.1');
  if (req.method === 'GET' && url.pathname === '/__reset') {
    TREE = buildTree();
    ota = { state: 0, error: 0, progress: 0, version: '' };
    return json(res, 200, { ok: true });
  }
  if (req.method === 'GET' && url.pathname === '/') {
    res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8', 'Cache-Control': 'no-store' });
    res.end(fs.readFileSync(PAGE));
    return;
  }
  const chunks = [];
  req.on('data', (c) => chunks.push(c));
  req.on('end', () => {
    const body = Buffer.concat(chunks);
    const isForm = (req.headers['content-type'] || '').includes('urlencoded');
    const form = isForm ? new URLSearchParams(body.toString('utf8')) : new URLSearchParams();
    const authed = req.headers['x-admin-token'] === TOKEN;
    const route = url.pathname;

    if (route === '/api/login') {
      if (form.get('code') !== '123456') return text(res, 403, '验证码不正确');
      return json(res, 200, { token: TOKEN });
    }
    if (route === '/api/overview') {
      if (!authed) return text(res, 403, '请重新登录');
      return json(res, 200, {
        name: SETTINGS.name, version: '0.1.7', sd: true,
        total: 15686696960, used: 3650722201, battery: 76, time: '21:04'
      });
    }
    if (route === '/api/settings') {
      if (!authed) return text(res, 403, '请重新登录');
      return json(res, 200, SETTINGS);
    }
    if (route === '/api/save') {
      if (!authed) return text(res, 403, '请重新登录');
      SETTINGS.name = form.get('name');
      SETTINGS.sleep = Number(form.get('sleep'));
      SETTINGS.timeout = Number(form.get('timeout'));
      return json(res, 200, {});
    }
    if (route === '/api/close') { if (!authed) return text(res, 403, '请重新登录'); return json(res, 200, {}); }
    if (route === '/api/files') {
      if (!authed) return text(res, 403, '请重新登录');
      const p = form.get('path') || '/';
      if (!TREE[p]) return text(res, 404, '文件夹不可用');
      return json(res, 200, { files: TREE[p] });
    }
    if (route === '/api/download') {
      if (!authed) return text(res, 403, '请重新登录');
      res.writeHead(200, { 'Content-Type': 'application/octet-stream' });
      return res.end(Buffer.alloc(2048, 7));
    }
    if (route === '/api/file-action') {
      if (!authed) return text(res, 403, '请重新登录');
      const op = form.get('op'), p = form.get('path'), dest = form.get('dest');
      const parent = p.substring(0, p.lastIndexOf('/')) || '/';
      const name = p.substring(p.lastIndexOf('/') + 1);
      const list = TREE[parent] || [];
      if (op === 'mkdir') {
        if (TREE[p]) return text(res, 400, '操作失败；文件可能已存在或文件夹非空');
        TREE[p] = [];
        list.push(dir(name));
        return json(res, 200, {});
      }
      if (op === 'rename') {
        const row = list.find((r) => r.name === name);
        if (!row) return text(res, 400, '操作失败；文件可能已存在或文件夹非空');
        row.name = dest.substring(dest.lastIndexOf('/') + 1);
        if (row.dir) { TREE[dest] = TREE[p]; delete TREE[p]; }
        return json(res, 200, {});
      }
      if (op === 'delete') {
        const idx = list.findIndex((r) => r.name === name);
        if (idx < 0) return text(res, 400, '操作失败；文件可能已存在或文件夹非空');
        const row = list[idx];
        if (row.dir && (TREE[p] || []).length) return text(res, 400, '操作失败；文件可能已存在或文件夹非空');
        list.splice(idx, 1);
        if (row.dir) delete TREE[p];
        return json(res, 200, {});
      }
      return text(res, 400, '操作失败');
    }
    if (route === '/api/upload') {
      if (!authed) return text(res, 403, '请重新登录');
      const target = url.searchParams.get('path') || '';
      const parent = target.substring(0, target.lastIndexOf('/')) || '/';
      const name = decodeURIComponent(target.substring(target.lastIndexOf('/') + 1));
      const list = TREE[parent] || (TREE[parent] = []);
      if (list.some((r) => r.name === name)) return text(res, 400, '上传失败，文件已存在或路径受保护');
      const m = body.toString('latin1').match(/filename="([^"]*)"/);
      list.push(file(name || (m ? m[1] : 'upload.bin'), body.length));
      // 故意延迟，便于观察上传进度弹层
      return setTimeout(() => json(res, 200, {}), 1200);
    }
    if (route === '/api/online-status') return json(res, 200, ota);
    if (route === '/api/online') {
      if (!authed) return text(res, 403, '请重新登录');
      const action = form.get('action');
      if (action === 'check') ota = { state: 3, error: 0, progress: 0, version: '0.1.8' };
      if (action === 'download') ota = { state: 4, error: 0, progress: 12, version: '0.1.8' };
      if (action === 'cancel') ota = { state: 0, error: 0, progress: 0, version: '' };
      return json(res, 200, {});
    }
    if (route === '/api/local-upload') { if (!authed) return text(res, 403, '请重新登录'); return json(res, 200, {}); }
    if (route === '/api/local-apply') { if (!authed) return text(res, 403, '请重新登录'); return json(res, 200, {}); }
    if (route === '/api/local-cancel') { if (!authed) return text(res, 403, '请重新登录'); return json(res, 200, {}); }
    return text(res, 404, 'not found');
  });
});

server.listen(Number(process.argv[2] || 8123), '127.0.0.1', () => {
  console.log('mock device api on http://127.0.0.1:' + server.address().port);
});
