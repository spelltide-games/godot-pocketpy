import os
import argparse
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--config', type=str, default='Debug')
parser.add_argument('--platform', type=str, default='win32')
parser.add_argument('--cmake-arg', action='append', default=[], metavar='ARG',
                    help='Extra CMake configure argument; repeat --cmake-arg=-DNAME=VALUE as needed')

def run(cmd: list[str]):
    print(subprocess.list2cmdline(cmd), flush=True)
    subprocess.run(cmd, check=True)

args = parser.parse_args()
config: str = args.config
platform: str = args.platform

extra_flags = []

if platform == 'android':
    toolchain_file = os.path.join(os.environ['ANDROID_NDK_HOME'], 'build/cmake/android.toolchain.cmake')
    extra_flags.append('-DCMAKE_TOOLCHAIN_FILE=' + toolchain_file)
    extra_flags.append('-DANDROID_PLATFORM=android-22')
    extra_flags.append('-DANDROID_ABI=arm64-v8a')
elif platform == 'ios':
    toolchain_file = 'godot-cpp/cmake/ios.toolchain.cmake'
    toolchain_file = os.path.abspath(toolchain_file)
    extra_flags.append('-DCMAKE_TOOLCHAIN_FILE=' + toolchain_file)
    extra_flags.append('-DDEPLOYMENT_TARGET=13.0')
    extra_flags.append('-DPLATFORM=OS64')

if config == 'Release':
    extra_flags.append('-DGODOTCPP_TARGET=template_release')

run(['cmake', '-B', 'build', f'-DCMAKE_BUILD_TYPE={config}', *extra_flags, *args.cmake_arg])
run(['cmake', '--build', 'build', '--config', config])
