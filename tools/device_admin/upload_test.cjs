const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const html = fs.readFileSync(path.join(__dirname, 'index.html'), 'utf8');
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
new vm.Script(script);

// 选择器：必须允许选择任意文件，且不设置 capture（否则安卓只给相机/照片）
assert.match(html, /id="file" accept="\*\/\*"/);
assert.doesNotMatch(html, /\bcapture=/);

// 结构：登录入口与四个分页仍在
assert.match(html, /id="code"/);
assert.match(html, /id="login"/);
assert.match(html, /data-tab="overview"/);
assert.match(html, /data-tab="files"/);
assert.match(html, /data-tab="settings"/);
assert.match(html, /data-tab="update"/);

// 不再使用 prompt/confirm：安卓 WebView 与热点登录页可能屏蔽原生对话框
assert.doesNotMatch(html, /\bprompt\s*\(/, '不应使用 prompt');
assert.doesNotMatch(html, /(^|[^.\w])confirm\s*\(/, '不应使用 confirm');

// 文件操作只使用固件已实现的 op，避免调用不存在的接口
assert.doesNotMatch(html, /op\s*[:=]\s*['"](copy|move|touch)/, '不应使用未实现的 op');
assert.match(html, /\{ op: 'mkdir', path: /);
assert.match(html, /\{ op: 'rename', path: .*dest: /);
assert.match(html, /\{ op: 'delete', path: /);

// 上传目标路径必须编码，中文文件名不能把查询串写坏
assert.match(html, /'\/api\/upload\?path=' \+ encodeURIComponent\(target\)/);

let request;
class Request {
  constructor() { request = this; this.upload = {}; }
  open(method, url) { this.method = method; this.url = url; }
  setRequestHeader(key, value) { this.header = [key, value]; }
  send(data) { this.body = data; }
}
const context = vm.createContext({ XMLHttpRequest: Request, token: 'test-token' });
vm.runInContext(script.slice(script.indexOf('function transfer('), script.indexOf('\nasync function upload()')), context);
(async () => {
  const progress = [];
  let finished = false;
  const operation = context.transfer('/api/upload?path=%2Ftest.mp3', 'body', (...args) => progress.push(args));
  operation.then(() => { finished = true; });
  assert.deepEqual(request.header, ['X-Admin-Token', 'test-token']);
  request.upload.onprogress({ loaded: 50, total: 100, lengthComputable: true });
  assert.deepEqual(progress.pop(), [50, 100]);
  request.upload.onload();
  await Promise.resolve();
  assert.equal(finished, false, '100% sent must wait for device response');
  request.status = 200; request.onload(); await operation;
  const failed = context.transfer('/api/upload', 'body', () => {});
  request.status = 400; request.responseText = '文件已存在'; request.onload();
  await assert.rejects(failed, /文件已存在/);
  const disconnected = context.transfer('/api/upload', 'body', () => {});
  request.onerror(); await assert.rejects(disconnected, /连接中断/);
  console.log('PASS: 脚本语法、任意文件选择、接口约束、进度、服务端确认、失败与断连处理');
})().catch(error => { console.error(error); process.exitCode = 1; });
