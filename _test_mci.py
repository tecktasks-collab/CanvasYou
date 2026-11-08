import sqlite3, os, ctypes
from ctypes import wintypes

p = os.path.join(os.environ["APPDATA"], r"PresentationSoftwareDev\app.sqlite")
c = sqlite3.connect(p)
c.text_factory = lambda b: b.decode("mbcs", "replace")
rows = c.execute("select id,name,path from audio_tracks limit 5").fetchall()
root = os.path.join(os.environ["APPDATA"], "PresentationSoftwareDev")
winmm = ctypes.WinDLL("winmm")
mci = winmm.mciSendStringW
mci.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_uint, ctypes.c_void_p]
GetShortPathNameW = ctypes.windll.kernel32.GetShortPathNameW
buf = (ctypes.c_wchar * 512)()

for r in rows:
    full = os.path.join(root, r[2].replace("/", "\\"))
    print("FILE", r[0], r[1], "exists", os.path.exists(full))
    short = (ctypes.c_wchar * 512)()
    n = GetShortPathNameW(full, short, 512)
    print(" short", n, short.value if n else "")
    for path in [full, short.value if n else full]:
        for cmd in [
            f'open "{path}" type mpegvideo alias t1',
            f'open "{path}" alias t1',
        ]:
            mci("close t1", None, 0, None)
            err = mci(cmd, buf, 512, None)
            print(" ", err, cmd[:100])
            if err == 0:
                mci("play t1", None, 0, None)
                mci("stop t1", None, 0, None)
                mci("close t1", None, 0, None)
                print("  OK")
                break
        else:
            continue
        break
