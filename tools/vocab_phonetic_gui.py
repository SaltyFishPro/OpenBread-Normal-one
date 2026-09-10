#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Vocabulary phonetic completer GUI.

Input vocabulary format:
    word<TAB>meaning

Output format:
    word<TAB>phonetic<TAB>meaning
"""

from __future__ import annotations

import json
import queue
import re
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
import tkinter as tk
from tkinter import filedialog, messagebox, ttk


SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_VOCAB_DIR = Path(r"C:\Users\YuChu\Desktop\english-vocabulary-master")
DEFAULT_DICT_FILE = DEFAULT_VOCAB_DIR / "英汉大词典_edited（232686）.txt"
DEFAULT_OUTPUT_DIR = DEFAULT_VOCAB_DIR / "phonetic_output"
DEFAULT_CACHE_FILE = SCRIPT_DIR / "cache" / "phonetic_online_cache.txt"
DICT_API_URL = "https://api.dictionaryapi.dev/api/v2/entries/en/"
WIKTIONARY_API_URL = "https://en.wiktionary.org/w/api.php"


ENTRY_SEPARATOR = "⬄"
PHONETIC_RE = re.compile(r"(?:^|\s)/([^/\r\n]{1,120})/")
WIKTIONARY_IPA_TEMPLATE_RE = re.compile(r"\{\{IPA(?:char)?\|en\|([^{}\|\r\n]+)")
WIKTIONARY_SLASH_IPA_RE = re.compile(r"/([^/\r\n]{1,120})/")
SUPERSCRIPT_DIGITS = str.maketrans("", "", "¹²³⁴⁵⁶⁷⁸⁹⁰")


def normalize_word(value: str) -> str:
    text = value.strip().lstrip("*").strip()
    text = text.translate(SUPERSCRIPT_DIGITS)
    text = text.replace("·", "")
    text = text.strip(" \t\r\n,;，；:：.。()[]{}<>\"'“”‘’")
    return text.lower()


def split_headwords(value: str) -> list[str]:
    parts = re.split(r"[,，;；]| 或 | or ", value)
    words: list[str] = []
    for part in parts:
        item = normalize_word(part)
        if item:
            words.append(item)
    return words


def parse_dictionary_line(line: str) -> tuple[list[str], str] | None:
    line = line.strip()
    if not line or ENTRY_SEPARATOR not in line:
        return None

    left, right = line.split(ENTRY_SEPARATOR, 1)
    match = PHONETIC_RE.search(right)
    if not match:
        return None

    phonetic = match.group(1).strip()
    if not phonetic:
        return None

    words = split_headwords(left)

    alias_part = right[: match.start()].strip()
    if alias_part:
        words.extend(split_headwords(alias_part))

    unique_words: list[str] = []
    seen: set[str] = set()
    for word in words:
        if word and word not in seen:
            seen.add(word)
            unique_words.append(word)

    if not unique_words:
        return None
    return unique_words, phonetic


def build_phonetic_map(dict_path: Path, post) -> dict[str, str]:
    file_size = max(dict_path.stat().st_size, 1)
    phonetic_map: dict[str, str] = {}
    parsed = 0
    bytes_read = 0
    last_report = 0.0

    with dict_path.open("rb") as handle:
        for raw_line in handle:
            bytes_read += len(raw_line)
            line = raw_line.decode("utf-8", errors="replace")
            parsed_line = parse_dictionary_line(line)
            if parsed_line:
                words, phonetic = parsed_line
                for word in words:
                    phonetic_map.setdefault(word, phonetic)
                parsed += 1

            now = time.monotonic()
            if now - last_report > 0.08:
                last_report = now
                post(
                    "dict_progress",
                    min(100.0, bytes_read * 100.0 / file_size),
                    f"读取主词典：{parsed} 条音标",
                )

    post("dict_progress", 100.0, f"主词典完成：{len(phonetic_map)} 个索引")
    return phonetic_map


def candidate_keys(word: str) -> list[str]:
    base = normalize_word(word)
    if not base:
        return []

    keys = [base]

    def add(value: str) -> None:
        value = normalize_word(value)
        if value and value not in keys:
            keys.append(value)

    if base.endswith("ies") and len(base) > 3:
        add(base[:-3] + "y")
    if base.endswith("ied") and len(base) > 3:
        add(base[:-3] + "y")
    if base.endswith("ing") and len(base) > 5:
        stem = base[:-3]
        add(stem)
        if len(stem) > 2 and stem[-1] == stem[-2]:
            add(stem[:-1])
        add(stem + "e")
    if base.endswith("ed") and len(base) > 4:
        stem = base[:-2]
        add(stem)
        add(stem + "e")
        if len(stem) > 2 and stem[-1] == stem[-2]:
            add(stem[:-1])
    if base.endswith("es") and len(base) > 3:
        add(base[:-2])
    if base.endswith("s") and len(base) > 2:
        add(base[:-1])

    return keys


def lookup_phonetic(word: str, phonetic_map: dict[str, str]) -> str:
    for key in candidate_keys(word):
        phonetic = phonetic_map.get(key)
        if phonetic:
            return phonetic
    return ""


def lookup_cached_online_phonetic(word: str, online_cache: dict[str, str]) -> str:
    for key in online_candidate_words(word):
        phonetic = online_cache.get(key)
        if phonetic:
            return phonetic
    return ""


def all_online_candidates_missed(word: str, online_misses: set[str]) -> bool:
    candidates = online_candidate_words(word)
    return bool(candidates) and all(candidate in online_misses for candidate in candidates)


def bracket_phonetic(phonetic: str) -> str:
    text = phonetic.strip()
    if not text:
        return ""
    text = text.strip("[]/")
    return f"[{text}]" if text else ""


def load_online_cache(cache_path: Path) -> tuple[dict[str, str], set[str]]:
    cache: dict[str, str] = {}
    misses: set[str] = set()
    if not cache_path.is_file():
        return cache, misses

    with cache_path.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) < 2:
                continue
            word = normalize_word(parts[0])
            phonetic = parts[1].strip()
            if word and phonetic:
                cache[word] = phonetic
            elif word and len(parts) >= 3 and parts[2].strip() == "miss":
                misses.add(word)
    return cache, misses


def append_online_cache(cache_path: Path, word: str, phonetic: str, source: str) -> None:
    cache_path.parent.mkdir(parents=True, exist_ok=True)
    with cache_path.open("a", encoding="utf-8", newline="\n") as handle:
        handle.write(f"{normalize_word(word)}\t{bracket_phonetic(phonetic)}\t{source}\n")


def append_online_miss(cache_path: Path, word: str) -> None:
    cache_path.parent.mkdir(parents=True, exist_ok=True)
    with cache_path.open("a", encoding="utf-8", newline="\n") as handle:
        handle.write(f"{normalize_word(word)}\t\tmiss\n")


def normalize_online_phonetic(value: str) -> str:
    text = value.strip()
    if not text:
        return ""
    text = text.strip("[]/")
    text = text.split(",", 1)[0].strip()
    return text


def extract_phonetic_from_dictionary_api(payload: object) -> str:
    if not isinstance(payload, list):
        return ""

    for entry in payload:
        if not isinstance(entry, dict):
            continue

        phonetic = normalize_online_phonetic(str(entry.get("phonetic", "")))
        if phonetic:
            return phonetic

        phonetics = entry.get("phonetics", [])
        if not isinstance(phonetics, list):
            continue
        for item in phonetics:
            if not isinstance(item, dict):
                continue
            phonetic = normalize_online_phonetic(str(item.get("text", "")))
            if phonetic:
                return phonetic
    return ""


def query_online_phonetic(word: str, timeout_sec: float) -> str:
    key = normalize_word(word)
    if not key:
        return ""

    url = DICT_API_URL + urllib.parse.quote(key)
    request = urllib.request.Request(url, headers={"User-Agent": "OpenBread-VocabTool/1.0"})
    try:
        with urllib.request.urlopen(request, timeout=timeout_sec) as response:
            if response.status != 200:
                return ""
            payload = json.loads(response.read().decode("utf-8", errors="replace"))
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError, OSError):
        return ""

    return extract_phonetic_from_dictionary_api(payload)


def extract_phonetic_from_wiktionary_wikitext(wikitext: str) -> str:
    for match in WIKTIONARY_IPA_TEMPLATE_RE.finditer(wikitext):
        phonetic = normalize_online_phonetic(match.group(1))
        if phonetic:
            return phonetic

    english_pos = wikitext.find("==English==")
    if english_pos >= 0:
        next_lang = wikitext.find("\n==", english_pos + len("==English=="))
        english_text = wikitext[english_pos: next_lang if next_lang >= 0 else len(wikitext)]
        pronunciation_pos = english_text.find("===Pronunciation===")
        if pronunciation_pos >= 0:
            next_section = english_text.find("\n===", pronunciation_pos + len("===Pronunciation==="))
            pronunciation_text = english_text[pronunciation_pos: next_section if next_section >= 0 else len(english_text)]
            match = WIKTIONARY_SLASH_IPA_RE.search(pronunciation_text)
            if match:
                return normalize_online_phonetic(match.group(1))
    return ""


def query_wiktionary_phonetic(word: str, timeout_sec: float) -> str:
    key = normalize_word(word)
    if not key:
        return ""

    params = {
        "action": "query",
        "format": "json",
        "prop": "revisions",
        "rvprop": "content",
        "rvslots": "main",
        "titles": key,
    }
    url = WIKTIONARY_API_URL + "?" + urllib.parse.urlencode(params)
    request = urllib.request.Request(url, headers={"User-Agent": "OpenBread-VocabTool/1.0"})
    try:
        with urllib.request.urlopen(request, timeout=timeout_sec) as response:
            if response.status != 200:
                return ""
            payload = json.loads(response.read().decode("utf-8", errors="replace"))
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError, OSError):
        return ""

    pages = payload.get("query", {}).get("pages", {})
    if not isinstance(pages, dict):
        return ""

    for page in pages.values():
        if not isinstance(page, dict) or "missing" in page:
            continue
        revisions = page.get("revisions", [])
        if not revisions:
            continue
        revision = revisions[0]
        if not isinstance(revision, dict):
            continue
        slots = revision.get("slots", {})
        main = slots.get("main", {}) if isinstance(slots, dict) else {}
        wikitext = main.get("*", "") if isinstance(main, dict) else revision.get("*", "")
        if not isinstance(wikitext, str):
            continue
        phonetic = extract_phonetic_from_wiktionary_wikitext(wikitext)
        if phonetic:
            return phonetic
    return ""


def online_candidate_words(word: str) -> list[str]:
    keys = candidate_keys(word)
    base = normalize_word(word)

    def add(value: str) -> None:
        value = normalize_word(value)
        if value and value not in keys:
            keys.append(value)

    if "-" in base:
        add(base.replace("-", " "))
        add(base.replace("-", ""))
    if " " in base:
        add(base.replace(" ", "-"))
        add(base.replace(" ", ""))
    if "'" in base:
        add(base.replace("'", ""))
    return keys


def query_online_sources(word: str, timeout_sec: float) -> tuple[str, str, str]:
    for candidate in online_candidate_words(word):
        phonetic = query_online_phonetic(candidate, timeout_sec)
        if phonetic:
            return phonetic, "dictionaryapi", candidate

        phonetic = query_wiktionary_phonetic(candidate, timeout_sec)
        if phonetic:
            return phonetic, "wiktionary", candidate

    return "", "", ""


def discover_vocab_files(input_dir: Path, dict_path: Path, output_dir: Path) -> list[Path]:
    files: list[Path] = []
    dict_resolved = dict_path.resolve()
    output_resolved = output_dir.resolve()

    for path in sorted(input_dir.glob("*.txt")):
        try:
            resolved = path.resolve()
        except OSError:
            continue
        if resolved == dict_resolved:
            continue
        if output_resolved in resolved.parents:
            continue
        if path.name in {"unmatched_words.txt", "summary.txt"}:
            continue
        if path.stem.endswith("_音标补全"):
            continue
        files.append(path)
    return files


def parse_vocab_line(line: str) -> tuple[str, str] | None:
    text = line.strip("\ufeff\r\n")
    if not text.strip():
        return None

    if "\t" in text:
        word, meaning = text.split("\t", 1)
    else:
        parts = text.split(maxsplit=1)
        if len(parts) == 1:
            word, meaning = parts[0], ""
        else:
            word, meaning = parts

    word = word.strip()
    meaning = meaning.strip()
    if not word:
        return None
    return word, meaning


def process_vocab_file(
    path: Path,
    output_dir: Path,
    phonetic_map: dict[str, str],
    online_cache: dict[str, str],
    online_misses: set[str],
    use_online: bool,
    request_delay_sec: float,
    request_timeout_sec: float,
    cache_path: Path,
    post,
) -> tuple[int, int, int, list[str]]:
    total_lines = sum(1 for _ in path.open("r", encoding="utf-8", errors="replace"))
    output_path = output_dir / f"{path.stem}_音标补全.txt"
    unmatched: list[str] = []
    total_entries = 0
    local_matched_entries = 0
    online_matched_entries = 0

    with path.open("r", encoding="utf-8", errors="replace") as src, output_path.open("w", encoding="utf-8", newline="\n") as dst:
        for index, line in enumerate(src, start=1):
            parsed = parse_vocab_line(line)
            if not parsed:
                continue

            word, meaning = parsed
            phonetic = lookup_phonetic(word, phonetic_map)
            if phonetic:
                local_matched_entries += 1
            else:
                cache_key = normalize_word(word)
                cached = lookup_cached_online_phonetic(word, online_cache)
                if cached:
                    phonetic = cached
                    online_matched_entries += 1
                elif use_online and not all_online_candidates_missed(word, online_misses):
                    post("file_progress", 100.0 if total_lines == 0 else index * 100.0 / total_lines, f"联网查询：{word}")
                    phonetic, source, matched_candidate = query_online_sources(word, request_timeout_sec)
                    if phonetic:
                        phonetic = bracket_phonetic(phonetic)
                        online_cache[normalize_word(matched_candidate)] = phonetic
                        if cache_key:
                            online_cache[cache_key] = phonetic
                        append_online_cache(cache_path, matched_candidate or word, phonetic, source or "online")
                        if matched_candidate and normalize_word(matched_candidate) != cache_key:
                            append_online_cache(cache_path, word, phonetic, source or "online")
                        online_matched_entries += 1
                    else:
                        for candidate in online_candidate_words(word):
                            online_misses.add(candidate)
                            append_online_miss(cache_path, candidate)
                        unmatched.append(word)
                    if request_delay_sec > 0:
                        time.sleep(request_delay_sec)
                else:
                    unmatched.append(word)

            total_entries += 1
            dst.write(f"{word}\t{bracket_phonetic(phonetic)}\t{meaning}\n")

            if index % 100 == 0 or index == total_lines:
                progress = 100.0 if total_lines == 0 else index * 100.0 / total_lines
                post("file_progress", progress, f"{path.name}：{index}/{total_lines}")

    post("file_progress", 100.0, f"{path.name} 完成")
    return total_entries, local_matched_entries, online_matched_entries, unmatched


def run_job(
    dict_path: Path,
    input_dir: Path,
    output_dir: Path,
    use_online: bool,
    request_delay_sec: float,
    request_timeout_sec: float,
    cache_path: Path,
    post,
) -> None:
    if not dict_path.is_file():
        raise FileNotFoundError(f"主词典不存在：{dict_path}")
    if not input_dir.is_dir():
        raise FileNotFoundError(f"词库目录不存在：{input_dir}")

    output_dir.mkdir(parents=True, exist_ok=True)
    post("log", f"输出目录：{output_dir}")
    post("log", f"联网补全：{'开启' if use_online else '关闭'}")

    phonetic_map = build_phonetic_map(dict_path, post)
    online_cache, online_misses = load_online_cache(cache_path)
    if use_online:
        post("log", f"联网缓存：{cache_path}（命中 {len(online_cache)}，失败 {len(online_misses)}）")
    vocab_files = discover_vocab_files(input_dir, dict_path, output_dir)
    if not vocab_files:
        raise RuntimeError("没有找到需要处理的 .txt 词库文件")

    post("log", f"待处理文件：{len(vocab_files)} 个")

    grand_total = 0
    grand_local_matched = 0
    grand_online_matched = 0
    unmatched_records: list[str] = []

    for file_index, path in enumerate(vocab_files, start=1):
        post("log", f"开始处理：{path.name}")
        total, local_matched, online_matched, unmatched = process_vocab_file(
            path,
            output_dir,
            phonetic_map,
            online_cache,
            online_misses,
            use_online,
            request_delay_sec,
            request_timeout_sec,
            cache_path,
            post,
        )
        grand_total += total
        grand_local_matched += local_matched
        grand_online_matched += online_matched
        unmatched_records.extend(f"{word}\t{path.name}" for word in unmatched)
        post("total_progress", file_index * 100.0 / len(vocab_files), f"总进度：{file_index}/{len(vocab_files)}")
        post("log", f"{path.name}：本地 {local_matched}，联网 {online_matched}，总计 {local_matched + online_matched}/{total}")

    unmatched_path = output_dir / "unmatched_words.txt"
    unmatched_unique = sorted(set(unmatched_records), key=str.lower)
    unmatched_path.write_text("\n".join(unmatched_unique) + ("\n" if unmatched_unique else ""), encoding="utf-8")

    summary = [
        f"主词典：{dict_path}",
        f"输入目录：{input_dir}",
        f"输出目录：{output_dir}",
        f"处理词条：{grand_total}",
        f"本地命中：{grand_local_matched}",
        f"联网命中：{grand_online_matched}",
        f"命中音标：{grand_local_matched + grand_online_matched}",
        f"未命中：{grand_total - grand_local_matched - grand_online_matched}",
        f"命中率：{((grand_local_matched + grand_online_matched) * 100.0 / grand_total if grand_total else 0):.2f}%",
    ]
    (output_dir / "summary.txt").write_text("\n".join(summary) + "\n", encoding="utf-8")
    post("done", "\n".join(summary))


class App(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("单词音标补全工具")
        self.geometry("780x540")
        self.minsize(720, 500)

        self.events: queue.Queue[tuple] = queue.Queue()
        self.worker: threading.Thread | None = None

        self.dict_var = tk.StringVar(value=str(DEFAULT_DICT_FILE))
        self.input_var = tk.StringVar(value=str(DEFAULT_VOCAB_DIR))
        self.output_var = tk.StringVar(value=str(DEFAULT_OUTPUT_DIR))
        self.use_online_var = tk.BooleanVar(value=False)
        self.delay_var = tk.StringVar(value="0.3")
        self.timeout_var = tk.StringVar(value="5.0")
        self.cache_var = tk.StringVar(value=str(DEFAULT_CACHE_FILE))
        self.status_var = tk.StringVar(value="等待开始")

        self._build_ui()
        self.after(80, self._poll_events)

    def _build_ui(self) -> None:
        root = ttk.Frame(self, padding=12)
        root.pack(fill=tk.BOTH, expand=True)
        root.columnconfigure(1, weight=1)
        root.rowconfigure(10, weight=1)

        self._path_row(root, 0, "主词典", self.dict_var, self._browse_dict)
        self._path_row(root, 1, "词库目录", self.input_var, self._browse_input)
        self._path_row(root, 2, "输出目录", self.output_var, self._browse_output)
        self._path_row(root, 3, "联网缓存", self.cache_var, self._browse_cache)

        options = ttk.Frame(root)
        options.grid(row=4, column=0, columnspan=3, sticky="ew", pady=(8, 2))
        options.columnconfigure(5, weight=1)
        ttk.Checkbutton(options, text="联网多源查找未命中音标", variable=self.use_online_var).grid(row=0, column=0, sticky="w")
        ttk.Label(options, text="请求间隔(s)").grid(row=0, column=1, sticky="w", padx=(18, 4))
        ttk.Entry(options, textvariable=self.delay_var, width=8).grid(row=0, column=2, sticky="w")
        ttk.Label(options, text="超时(s)").grid(row=0, column=3, sticky="w", padx=(18, 4))
        ttk.Entry(options, textvariable=self.timeout_var, width=8).grid(row=0, column=4, sticky="w")

        self.start_button = ttk.Button(root, text="开始补全", command=self._start)
        self.start_button.grid(row=5, column=0, sticky="w", pady=(10, 6))

        ttk.Label(root, textvariable=self.status_var).grid(row=5, column=1, columnspan=2, sticky="w", pady=(10, 6))

        ttk.Label(root, text="主词典").grid(row=6, column=0, sticky="w")
        self.dict_progress = ttk.Progressbar(root, maximum=100)
        self.dict_progress.grid(row=6, column=1, columnspan=2, sticky="ew", pady=4)

        ttk.Label(root, text="当前文件").grid(row=7, column=0, sticky="w")
        self.file_progress = ttk.Progressbar(root, maximum=100)
        self.file_progress.grid(row=7, column=1, columnspan=2, sticky="ew", pady=4)

        ttk.Label(root, text="总进度").grid(row=8, column=0, sticky="w")
        self.total_progress = ttk.Progressbar(root, maximum=100)
        self.total_progress.grid(row=8, column=1, columnspan=2, sticky="ew", pady=4)

        ttk.Label(root, text="日志").grid(row=9, column=0, sticky="nw", pady=(8, 2))
        self.log_text = tk.Text(root, height=14, wrap="word")
        self.log_text.grid(row=10, column=0, columnspan=3, sticky="nsew")

        scroll = ttk.Scrollbar(root, orient=tk.VERTICAL, command=self.log_text.yview)
        scroll.grid(row=10, column=3, sticky="ns")
        self.log_text.configure(yscrollcommand=scroll.set)

    def _path_row(self, parent: ttk.Frame, row: int, label: str, var: tk.StringVar, command) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky="w", pady=3)
        ttk.Entry(parent, textvariable=var).grid(row=row, column=1, sticky="ew", padx=8, pady=3)
        ttk.Button(parent, text="选择", command=command).grid(row=row, column=2, sticky="e", pady=3)

    def _browse_dict(self) -> None:
        path = filedialog.askopenfilename(title="选择主词典", filetypes=[("Text files", "*.txt"), ("All files", "*.*")])
        if path:
            self.dict_var.set(path)

    def _browse_input(self) -> None:
        path = filedialog.askdirectory(title="选择待处理词库目录")
        if path:
            self.input_var.set(path)

    def _browse_output(self) -> None:
        path = filedialog.askdirectory(title="选择输出目录")
        if path:
            self.output_var.set(path)

    def _browse_cache(self) -> None:
        path = filedialog.asksaveasfilename(
            title="选择联网缓存文件",
            defaultextension=".txt",
            filetypes=[("Text files", "*.txt"), ("All files", "*.*")],
        )
        if path:
            self.cache_var.set(path)

    def _post(self, *item) -> None:
        self.events.put(item)

    def _start(self) -> None:
        if self.worker and self.worker.is_alive():
            return

        self.log_text.delete("1.0", tk.END)
        self.dict_progress["value"] = 0
        self.file_progress["value"] = 0
        self.total_progress["value"] = 0
        self.status_var.set("处理中...")
        self.start_button.configure(state=tk.DISABLED)

        dict_path = Path(self.dict_var.get().strip())
        input_dir = Path(self.input_var.get().strip())
        output_dir = Path(self.output_var.get().strip())
        cache_path = Path(self.cache_var.get().strip())
        try:
            request_delay_sec = max(0.0, float(self.delay_var.get().strip()))
            request_timeout_sec = max(1.0, float(self.timeout_var.get().strip()))
        except ValueError:
            messagebox.showerror("错误", "请求间隔和超时必须是数字")
            self.start_button.configure(state=tk.NORMAL)
            self.status_var.set("等待开始")
            return
        use_online = self.use_online_var.get()

        def target() -> None:
            try:
                run_job(
                    dict_path,
                    input_dir,
                    output_dir,
                    use_online,
                    request_delay_sec,
                    request_timeout_sec,
                    cache_path,
                    self._post,
                )
            except Exception as exc:  # noqa: BLE001 - show GUI error without crashing Tk.
                self._post("error", str(exc))

        self.worker = threading.Thread(target=target, daemon=True)
        self.worker.start()

    def _append_log(self, text: str) -> None:
        self.log_text.insert(tk.END, text + "\n")
        self.log_text.see(tk.END)

    def _poll_events(self) -> None:
        try:
            while True:
                item = self.events.get_nowait()
                kind = item[0]

                if kind == "log":
                    self._append_log(item[1])
                elif kind == "dict_progress":
                    self.dict_progress["value"] = item[1]
                    self.status_var.set(item[2])
                elif kind == "file_progress":
                    self.file_progress["value"] = item[1]
                    self.status_var.set(item[2])
                elif kind == "total_progress":
                    self.total_progress["value"] = item[1]
                    self.status_var.set(item[2])
                elif kind == "done":
                    self.dict_progress["value"] = 100
                    self.file_progress["value"] = 100
                    self.total_progress["value"] = 100
                    self.status_var.set("完成")
                    self._append_log("完成")
                    self._append_log(item[1])
                    self.start_button.configure(state=tk.NORMAL)
                    messagebox.showinfo("完成", item[1])
                elif kind == "error":
                    self.status_var.set("失败")
                    self._append_log("错误：" + item[1])
                    self.start_button.configure(state=tk.NORMAL)
                    messagebox.showerror("错误", item[1])
        except queue.Empty:
            pass

        self.after(80, self._poll_events)


if __name__ == "__main__":
    App().mainloop()
