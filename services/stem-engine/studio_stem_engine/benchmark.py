"""Run a local corpus manifest; reference-free results never become quality scores."""
import argparse
from dataclasses import asdict
import json
from pathlib import Path
import time
import tempfile
import numpy as np
from .engine import Engine, Settings, STEMS
from .audio import read


def metrics(reference, estimate):
    reference = reference.astype(np.float64)
    estimate = estimate.astype(np.float64)
    if reference.shape != estimate.shape:
        raise ValueError('Reference and estimate must share the exact sample grid')
    ref = reference - reference.mean(axis=0, keepdims=True)
    est = estimate - estimate.mean(axis=0, keepdims=True)
    power = float(np.sum(ref * ref))
    if power < 1e-12:
        return {'si_sdr_db': None, 'snr_db': None, 'reason': 'silent reference',
                'estimate_rms': float(np.sqrt(np.mean(estimate**2)))}
    target = ref * (np.sum(ref*est) / power)
    si = 10*np.log10((np.sum(target**2)+1e-12)/(np.sum((est-target)**2)+1e-12))
    snr = 10*np.log10((np.sum(reference**2)+1e-12)/(np.sum((estimate-reference)**2)+1e-12))
    return {'si_sdr_db': float(si), 'snr_db': float(snr)}


def benchmark(manifest_path, output):
    manifest_path=Path(manifest_path).resolve()
    manifest=json.loads(manifest_path.read_text())
    output=Path(output).resolve(); output.mkdir(parents=True,exist_ok=True)
    report={'schema':'studio.benchmark.v1', 'purpose':manifest['purpose'], 'runs':[]}
    for item in manifest['tracks']:
        source=(manifest_path.parent/item['mixture']).resolve()
        mixture, rate=read(source)
        for config in manifest['configurations']:
            settings=Settings(**config['settings'])
            row={'track':item['id'],'configuration':config['name'],'settings':asdict(settings)}
            began=time.monotonic()
            try:
                # Unique output roots force actual inference instead of output-cache timings.
                # Model downloads/initialization remain in wall time and are not mislabeled.
                with tempfile.TemporaryDirectory(prefix='run-',dir=output) as run:
                    result=Engine(run).separate(source, settings)
                    metadata=Engine.verify(result)
                    row.update(seconds=time.monotonic()-began,
                               duration_seconds=len(mixture)/rate,
                               metadata=metadata, status='ok')
                    row['realtime_factor']=row['seconds']/row['duration_seconds']
                    if 'references' in item:
                        row['metrics']={}
                        for stem in STEMS:
                            ref,sr=read((manifest_path.parent/item['references'][stem]).resolve())
                            est,er=read(result/f'{stem}.wav')
                            if sr!=er: raise ValueError('Reference sample rate differs; no implicit metric resampling')
                            row['metrics'][stem]=metrics(ref,est)
                    else:
                        row['quality_scores']=None
                        row['quality_note']='No isolated references. Runtime/correctness only.'
            except Exception as exc:
                row.update(status='error', error=str(exc), seconds=time.monotonic()-began)
            report['runs'].append(row)
            (output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest');parser.add_argument('--output',default='benchmark-results')
    args=parser.parse_args()
    report=benchmark(args.manifest,args.output)
    print(json.dumps({'runs':len(report['runs']),'report':str(Path(args.output)/'report.json')}))
    return int(any(row['status']!='ok' for row in report['runs']))

if __name__=='__main__':
    raise SystemExit(main())
