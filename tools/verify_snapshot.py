from pathlib import Path
import hashlib,json,sys,zipfile

def sha(data):return hashlib.sha256(data).hexdigest()
def canonical(data):
    try:data.decode("utf-8")
    except UnicodeDecodeError:return data
    return data.replace(b"\r\n",b"\n")

root=Path(__file__).resolve().parents[1]
if len(sys.argv)>1:
    with zipfile.ZipFile(sys.argv[1]) as archive:
        assert archive.testzip() is None,"ZIP CRC mismatch"
        manifest=json.loads(archive.read("REPOSITORY-SHA256.json"))
        expected=set(manifest)|{"REPOSITORY-SHA256.json"}
        assert set(archive.namelist())==expected,"Unexpected or missing archive files"
        for name,digest in manifest.items():assert sha(archive.read(name))==digest,name
        source=json.loads(archive.read("SOURCE-SHA256.json"))
        for name,digest in source.items():assert sha(archive.read("src/"+name))==digest,name
    print("Archive CRC and byte hashes verified:",len(manifest),"inventoried files;",len(source),"application files")
else:
    manifest=json.loads((root/"SOURCE-CONTENT-SHA256.json").read_text(encoding="utf-8"))
    for name,digest in manifest.items():assert sha(canonical((root/"src"/name).read_bytes()))==digest,name
    print("Application content hashes verified:",len(manifest),"files (UTF-8 CRLF/LF normalized)")
