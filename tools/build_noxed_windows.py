#!/usr/bin/env python3
"""Use prebuilt Windows LLVM-MinGW + Arm GNU toolchains with the unchanged SDK builder.

python tools/build_noxed_windows.py --llvm-dir PATH --arm32-dir PATH --arm64-dir PATH plugins/Acidline
On other systems, or when Zig works, use the SDK's normal tools/build.py instead.
No compiler binaries are included in the source distribution.
"""
import argparse,sys,os
from pathlib import Path
import build
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 ap.add_argument('--llvm-dir',type=Path,required=True);ap.add_argument('--arm32-dir',type=Path,required=True);ap.add_argument('--arm64-dir',type=Path,required=True)
 args,rest=ap.parse_known_args()
 native=args.llvm_dir.resolve()/'bin'/'clang++.exe'
 if not native.is_file():ap.error(f'Missing {native}')
 build.find_system_cxx=lambda:[str(native),'-static']
 for target,base,triple in [('arm32',args.arm32_dir,'arm-none-linux-gnueabihf'),('arm64',args.arm64_dir,'aarch64-none-linux-gnu')]:
  compiler=base.resolve()/'bin'/f'{triple}-g++.exe'
  if not compiler.is_file():ap.error(f'Missing {compiler}')
  entry=list(build.ARM_TARGETS[target]);entry[2]=str(compiler);build.ARM_TARGETS[target]=tuple(entry)
 temp=Path(build.SDK_TOOLS_DIR)/'tmp';temp.mkdir(parents=True,exist_ok=True);os.environ['TEMP']=os.environ['TMP']=str(temp)
 sys.argv=[sys.argv[0],*rest,'--toolchain','gcc']
 return build.main()
if __name__=='__main__':sys.exit(main())
