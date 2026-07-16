"""算 OneNET 产品级鉴权 token"""
import hashlib, base64, urllib.parse

res_plain = 'products/7Sp4dA99m3'
res = urllib.parse.quote(res_plain, safe='')
key = '6/HRq7d7o5/DvRet5Wm7wfcdWVVbw6Gbu5nRzTdjhVc'
et = 1893456000
method = 'md5'

sig = base64.b64encode(hashlib.md5((key + str(et)).encode('utf-8')).digest()).decode()
sign_enc = urllib.parse.quote(sig, safe='')
token = f'version=2018-10-31&res={res}&et={et}&method={method}&sign={sign_enc}'

print('Token:')
print(token)
print()
print('--- 解析 ---')
print(f'res   = {res_plain}')
print(f'key   = {key}')
print(f'et    = {et}  (2030-01-01)')
print(f'method= {method}')
print(f'sign  = {sig}')
