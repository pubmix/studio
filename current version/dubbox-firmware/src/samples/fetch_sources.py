"""Fetch only the bank's pinned CC0 originals and verify every source hash."""
from pathlib import Path
import argparse,json,hashlib,urllib.request,concurrent.futures
p=argparse.ArgumentParser();p.add_argument('--cache',type=Path,required=True);args=p.parse_args()
cache=args.cache.resolve();(cache/'raw').mkdir(parents=True,exist_ok=True)
m=json.loads((Path(__file__).resolve().parent/'source-manifest.json').read_text())
def fetch(x):
 path=cache/'raw'/(x['name']+'.wav')
 data=path.read_bytes() if path.exists() else urllib.request.urlopen(x['url'],timeout=90).read()
 assert len(data)==x['source_bytes']
 assert hashlib.sha256(data).hexdigest()==x['sha256']
 assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==x['git_blob_sha1']
 path.write_bytes(data)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(fetch,m['samples']))
(cache/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
(cache/'VCSL-LICENSE').write_bytes((Path(__file__).resolve().parent/'LICENSE-VCSL-CC0.txt').read_bytes())
print('All thirteen original recordings verified.')
