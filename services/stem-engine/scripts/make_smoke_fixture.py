"""Create the deterministic 12-second execution fixture. NOT a music quality benchmark."""
from pathlib import Path
import argparse
import json
import numpy as np
import soundfile as sf

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',default='smoke-input')
args=parser.parse_args()
root=Path(args.output);root.mkdir(parents=True,exist_ok=True)
rate=44100
t=np.arange(rate*12)/rate
mix=(.07*np.sin(2*np.pi*110*t)+.05*np.sin(2*np.pi*440*t)*(np.sin(2*np.pi*2*t)>0)
     +.025*np.random.default_rng(0).normal(size=len(t))*np.exp(-20*(t%.5)))
sf.write(root/'synthetic.wav',np.column_stack([mix,mix*.9]),rate,subtype='FLOAT')
manifest={'purpose':'Synthetic execution smoke only; no musical quality claim',
          'tracks':[{'id':'synthetic-12s','mixture':'synthetic.wav'}],
          'configurations':[{'name':'fast-cpu','settings':{'preset':'fast','shifts':0}}]}
(root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(root/'manifest.json')
