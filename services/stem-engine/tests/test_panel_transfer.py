import struct
import zlib
from urllib.parse import urlsplit, parse_qs
import pytest
from test_transfer import masters, network, READY, STEMS, export_transfer, upload_transfer, UploadError


def test_compressed_companion_transfer(tmp_path, network):
    dest=export_transfer(masters(tmp_path),tmp_path/'out',set_id=22)
    for role in STEMS:
        network.replies.extend([{**READY,'transfer_modes':['wav','deflate-blocks-v1']},{'ok':True,'name':'S22-'+role.upper()+'.wav'}])
    events=[]
    assert len(upload_transfer(dest,'http://dubbox.local',transfer_mode='deflate-blocks-v1',progress=events.append))==4
    for role,connection in zip(STEMS,network.instances[1::2]):
        query=parse_qs(urlsplit(connection.path).query)
        content=b''.join(connection.chunks)
        assert len(content)==int(connection.headers['Content-Length'])
        data=content.split(b'\r\n\r\n',1)[1].rsplit(b'\r\n--',1)[0]
        assert query['encoding']==['deflate-blocks-v1']
        assert len(data)==int(query['wire_size'][0])
        decoded=b'';offset=0
        while offset<len(data):
            size,raw=struct.unpack_from('<HH',data,offset);block=zlib.decompress(data[offset+4:offset+4+size]);assert len(block)==raw
            decoded+=block;offset+=4+size
        assert decoded==(dest/(role+'.wav')).read_bytes()
    assert all(event['wire_bytes']<event['bytes'] for event in events if event['stage']=='upload')


def test_compressed_legacy_receiver_rejected(tmp_path,network):
    dest=export_transfer(masters(tmp_path),tmp_path/'out',set_id=22)
    network.replies=[READY]
    with pytest.raises(UploadError,match='does not support compressed'):
        upload_transfer(dest,'http://dubbox.local',transfer_mode='deflate-blocks-v1')
    assert len(network.instances)==1
