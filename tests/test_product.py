#!/usr/bin/env python3
"""Configuration and installer failures must stop before replacing user files."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from product import DEFAULT_PRODUCT, load_product, generated_files
from prepare_sdk import install_managed
from wake_bundle import describe
from check_image import incomplete_archives

class ProductTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.addCleanup(self.tmp.cleanup)
        self.default = json.loads(DEFAULT_PRODUCT.read_text())
        self.bundle = json.loads((DEFAULT_PRODUCT.parent / self.default["wake"]["bundle"]).read_text())
        self.default["wake"]["bundle"] = "wake.json"
    def load(self, p=None, bundle=None):
        (self.root / "wake.json").write_text(json.dumps(self.bundle if bundle is None else bundle))
        f = self.root / "product.json"
        f.write_text(json.dumps(self.default if p is None else p))
        return load_product(f)
    def test_default(self):
        p = self.load()
        self.assertTrue(p["wake"]["enabled"])
        self.assertEqual(p["wake"]["label"], "你好云宝")
        files = generated_files(p)
        self.assertIn(b'CONFIG_IMPO_CHINA_RELAY=y', files["product.conf"])
        self.assertIn('你好云宝',files["impo_product_config.h"].decode())
    def test_custom_wake_has_no_yunbao_filter(self):
        b = copy.deepcopy(self.bundle)
        b.update(label="Hello Mini",keywords=["hello mini"])
        b["model"].update(file="mini-model.bin",sha256=hashlib.sha256(b"different model").hexdigest())
        p=self.load(bundle=b)
        h=generated_files(p)["impo_product_config.h"].decode()
        self.assertIn('#define IMPO_WAKE_KEYWORDS {"hello mini"}',h)
        self.assertNotIn('ni3 hao3 yun2 bao3',h)
    def test_mini2_latency_profiles(self):
        p=load_product(DEFAULT_PRODUCT.parent / 'arcs-mini2-latency-batch.json')
        self.assertEqual(p['wake']['label'], '小周小周')
        self.assertEqual(p['wake']['keywords'], ['xiao3 zhou1 xiao3 zhou1', '小周小周'])
        overlap=load_product(DEFAULT_PRODUCT.parent / 'arcs-mini2-latency-overlap.json')
        self.assertEqual(p['wake'], overlap['wake'])
        self.assertEqual(p['voice']['upload_mode'], 'batch')
        self.assertEqual(overlap['voice']['upload_mode'], 'overlap')
    def test_voice_defaults_and_validation(self):
        self.assertEqual(self.load()["voice"], {"upload_mode":"batch", "endpoint_pause_ms":1200})
        self.default["voice"] = {"upload_mode":"overlap", "endpoint_pause_ms":800}
        files=generated_files(self.load())
        self.assertIn(b'IMPO_VOICE_UPLOAD_OVERLAP true', files['impo_product_config.h'])
        self.assertIn(b'IMPO_VOICE_ENDPOINT_PAUSE_MS 800', files['impo_product_config.h'])
        for value in [{"upload_mode":"stream"},{"endpoint_pause_ms":True},
                      {"endpoint_pause_ms":599},{"endpoint_pause_ms":2001},
                      {"reply_mode":"sentence"}]:
            self.default["voice"]=value
            with self.assertRaises(ValueError): self.load()
    def test_playback_config(self):
        self.assertEqual(self.load()['playback'],{'mode':'complete','prefill_ms':800,'sentence_tts':False})
        self.default['playback']={'mode':'adaptive','prefill_ms':800,'sentence_tts':True}
        self.assertIn(b'IMPO_PLAYBACK_ADAPTIVE true',generated_files(self.load())['impo_product_config.h'])
        for value in [{'mode':'stream'},{'prefill_ms':True},{'prefill_ms':599},{'prefill_ms':2001},{'sentence_tts':'yes'}]:
            self.default['playback']=value
            with self.assertRaises(ValueError):self.load()
    def test_disabled_needs_no_resources(self):
        self.default["wake"]={"enabled":False}
        p=self.load()
        self.assertIn(b'IMPO_WAKE_ENABLED false',generated_files(p)["impo_product_config.h"])
        self.assertNotIn('Say ',p["instructions"])
    def test_interrupt_opt_in_requires_wake(self):
        self.assertFalse(self.load()["wake_interrupt"])
        self.default["wake_interrupt"]=True
        files=generated_files(self.load())
        self.assertIn(b'IMPO_WAKE_INTERRUPT true',files['impo_product_config.h'])
        self.assertIn(b'CONFIG_LISA_AUDIO_RECORD_INDIVIDUAL_GAIN=y',files['product.conf'])
        self.default["wake"]={"enabled":False}
        with self.assertRaises(ValueError):self.load()
        self.default["wake_interrupt"]="yes"
        with self.assertRaises(ValueError):self.load()
    def test_text_alone_rejected(self):
        self.default["wake"]["label"]="another phrase"
        with self.assertRaises(ValueError):self.load()
    def test_bundle_contracts(self):
        for edit in [lambda b:b.update(keywords=[]),lambda b:b.update(keywords=["x","x"]),
                     lambda b:b["model"].update(sha256="0"*64),lambda b:b["model"].update(file="../model.bin")]:
            b=copy.deepcopy(self.bundle);edit(b)
            with self.assertRaises(ValueError):self.load(bundle=b)
    def test_unknown_and_credential_fields_rejected(self):
        self.default["wifi_password"]="not-a-real-password"
        with self.assertRaises(ValueError):self.load()
    def test_names_and_strings(self):
        for k,v in [("id","../escape"),("continuous_dialogue","yes"),("product_id",123),("screen_title","😀"),("display_name","bad\x00")]:
            p=copy.deepcopy(self.default);p[k]=v
            with self.assertRaises(ValueError):self.load(p)
        p=copy.deepcopy(self.default);p["instructions"]='He said "hello".\nSecond line.'
        self.assertIn(b'He said \\"hello\\".\\nSecond line.',generated_files(self.load(p))["impo_product_config.h"])
    def test_missing_glyph_is_explicit(self):
        self.default["screen_title"]="喵"
        with self.assertRaisesRegex(ValueError,"--font"):generated_files(self.load())
    def test_resource_descriptor_from_bytes(self):
        model=self.root/"mini.bin"; wrap=self.root/"mini.json"
        model.write_bytes(b"ALOG_RES_HV100001\0synthetic-test-fixture")
        wrap.write_text('{"cae":{"chns":{"mic":1,"ref":1}}}')
        b=describe("mini-test","Hello Mini",["hello mini"],model,wrap)
        self.assertEqual(b["model"]["sha256"],hashlib.sha256(model.read_bytes()).hexdigest())
        self.assertEqual(self.load(bundle=b)["wake"]["keywords"],["hello mini"])
        model.write_bytes(b"ALOG_RES_HV10001\0incorrect-header")
        with self.assertRaises(ValueError):describe("id","Hello Mini",["hello mini"],model,wrap)
        model.write_text("a phrase is not an acoustic model")
        with self.assertRaises(ValueError):describe("id","Hello Mini",["hello mini"],model,wrap)
    def test_resource_descriptor_rejects_wrong_channels(self):
        model=self.root/"mini.bin";wrap=self.root/"mini.json"
        model.write_bytes(b"ALOG_RES_HV100001\0fixture")
        wrap.write_text('{"cae":{"chns":{"mic":2,"ref":1}}}')
        with self.assertRaises(ValueError):describe("id","Hello Mini",["hello mini"],model,wrap)
    def test_archive_integrity(self):
        good=self.root/"good.a";bad=self.root/"bad.a"
        good.write_bytes(b"!<arch>\n");bad.write_bytes(b"")
        self.assertEqual(incomplete_archives(self.root),[bad])
        bad.write_bytes(b"!<thin>\n");self.assertEqual(incomplete_archives(self.root),[])
    def test_managed_updates_preserve_external_edits(self):
        f="arcs/components/impo_arcs_mini/example.c"
        install_managed(self.root,{f:b"old"})
        t=self.root/f; stamp=t.stat().st_mtime_ns
        install_managed(self.root,{f:b"old"});self.assertEqual(t.stat().st_mtime_ns,stamp)
        install_managed(self.root,{f:b"new"});self.assertEqual(t.read_bytes(),b"new")
        t.write_bytes(b"user edit")
        with self.assertRaises(SystemExit):install_managed(self.root,{f:b"replace"})
        self.assertEqual(t.read_bytes(),b"user edit")
        self.assertFalse((self.root/"unrelated").exists())
    def test_known_legacy_overlay_migrates_without_git_history(self):
        repo=Path(__file__).resolve().parents[1]
        name="arcs/boards/arcs_mini_yunbao/board.h"
        target=self.root/name;target.parent.mkdir(parents=True)
        target.write_bytes((repo/"firmware/boards/arcs_mini_yunbao/board.h").read_bytes())
        install_managed(self.root,{name:b"new SDK overlay"})
        self.assertEqual(target.read_bytes(),b"new SDK overlay")

if __name__ == "__main__":unittest.main()
