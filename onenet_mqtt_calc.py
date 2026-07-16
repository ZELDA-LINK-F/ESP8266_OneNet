"""新平台 token 算 - 试多种算法找对的那个
   已知: sign = Mp9kS4i8Lu2u16UQ1UTuGw== (16字节 = md5)
   key 可能: access_key 字符串 / device_secret 字符串
   算法可能: md5(key+et) / hmac_md5(key, msg) / hmac_sha1(key, msg) / md5(msg)
"""
import hashlib, hmac, base64
from urllib.parse import quote

ak = '6/HRq7d7o5/DvRet5Wm7wfcdWVVbw6Gbu5nRzTdjhVc'
ds = 'dUQ2U1hNdTBIRmNVT25IenMyaDdUSHBIbzhUM1cxblM='
et = '1805693871'
res = 'products/7Sp4dA99m3/devices/dev1'
version = '2018-10-31'

expected = 'Mp9kS4i8Lu2u16UQ1UTuGw=='

tests = [
    ("md5(ak_str + et)",     lambda: base64.b64encode(hashlib.md5((ak + et).encode()).digest()).decode()),
    ("md5(ds_str + et)",     lambda: base64.b64encode(hashlib.md5((ds + et).encode()).digest()).decode()),
    ("md5(ak_str)",          lambda: base64.b64encode(hashlib.md5(ak.encode()).digest()).decode()),
    ("md5(ds_str)",          lambda: base64.b64encode(hashlib.md5(ds.encode()).digest()).decode()),
    ("md5(res + et)",        lambda: base64.b64encode(hashlib.md5((res + et).encode()).digest()).decode()),
    ("md5(res)",             lambda: base64.b64encode(hashlib.md5(res.encode()).digest()).decode()),
    ("md5(et)",              lambda: base64.b64encode(hashlib.md5(et.encode()).digest()).decode()),
    ("md5(et+res)",          lambda: base64.b64encode(hashlib.md5((et + res).encode()).digest()).decode()),
    ("md5(et+method+res+ver)", lambda: base64.b64encode(hashlib.md5(f'{et}\nmd5\n{res}\n{version}'.encode()).digest()).decode()),
    ("hmac_md5(ak, msg)",    lambda: base64.b64encode(hmac.new(ak.encode(), f'{et}\nmd5\n{res}\n{version}'.encode(), hashlib.md5).digest()).decode()),
    ("hmac_md5(ds, msg)",    lambda: base64.b64encode(hmac.new(ds.encode(), f'{et}\nmd5\n{res}\n{version}'.encode(), hashlib.md5).digest()).decode()),
    ("hmac_md5(ak, et)",     lambda: base64.b64encode(hmac.new(ak.encode(), et.encode(), hashlib.md5).digest()).decode()),
    ("hmac_md5(ds, et)",     lambda: base64.b64encode(hmac.new(ds.encode(), et.encode(), hashlib.md5).digest()).decode()),
    ("hmac_sha1(ak, et)",    lambda: base64.b64encode(hmac.new(ak.encode(), et.encode(), hashlib.sha1).digest()).decode()),
    ("hmac_sha1(ds, et)",    lambda: base64.b64encode(hmac.new(ds.encode(), et.encode(), hashlib.sha1).digest()).decode()),
    ("md5(ak + res)",        lambda: base64.b64encode(hashlib.md5((ak + res).encode()).digest()).decode()),
    ("md5(ds + res)",        lambda: base64.b64encode(hashlib.md5((ds + res).encode()).digest()).decode()),
    ("md5(ak + ds)",         lambda: base64.b64encode(hashlib.md5((ak + ds).encode()).digest()).decode()),
]

for name, fn in tests:
    try:
        sig = fn()
        match = "✓✓✓ 匹配!" if sig == expected else ""
        print(f"  {name:30s} = {sig} {match}")
    except Exception as e:
        print(f"  {name:30s} = ERROR: {e}")

print(f"\n期望: {expected}")
