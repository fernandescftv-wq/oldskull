#!/usr/bin/env python3
import os, sys, subprocess, shutil

os.chdir('/home/e/esp/esp32_p4_jc4880p433c_bsp')

# Clean build
if os.path.exists('build'):
    shutil.rmtree('build')

# Try multiple Python paths
python_paths = [
    '/home/e/.espressif/python_env/idf5.5_py3.11_env/bin/python',
    '/tmp/idf_venv_build/bin/python'
]

id_path='/home/e/esp/esp-idf'
for py in python_paths:
    if not os.path.exists(py):
        continue
    print(f"Trying {py}...")
    env = os.environ.copy()
    env['IDF_PATH'] = id_path
    env['IDF_PYTHON_ENV_PATH'] = os.path.dirname(py)
    env['PATH'] = os.path.dirname(py) + ':' + env.get('PATH', '')
    
    # Try to build
    result = subprocess.run(
        [py, '/home/e/esp/esp-idf/tools/idf.py', 'build', '--warning-level', '1'],
        env=env,
        capture_output=True,
        text=True,
        timeout=600
    )
    
    if result.returncode == 0:
        print(f"Build SUCCESS with {py}!")
        print(result.stdout[-1000:] if len(result.stdout) > 1000 else result.stdout)
        sys.exit(0)
    else:
        print(f"Failed with {py}")
        print("STDERR:", result.stderr[-500:] if result.stderr else 'none')

print("All Python paths failed. Using fallback method...")

# Fallback: Install dependencies and build
venv_path = '/tmp/idf_fallback_venv'
shutil.rmtree(venv_path, ignore_errors=True)
subprocess.run([sys.executable, '-m', 'venv', venv_path], check=True, capture_output=True)

pip = f'{venv_path}/bin/pip'
subprocess.run([pip, 'install', '--upgrade', 'pip'], capture_output=True)

# Install idf dependencies
subprocess.run([pip, 'install', 'scikit-build', 'ninja', 'psutil', 'pyserial'], capture_output=True)

# Build
env = os.environ.copy()
env['IDF_PATH'] = id_path
env['PATH'] = f'{venv_path}/bin:/home/e/.espressif/tools/riscv32-esp-elf/esp-14.2.0_20251107/riscv32-esp-elf/bin:{env.get("PATH", "")}'

result = subprocess.run(
    [f'{venv_path}/bin/python', '/home/e/esp/esp-idf/tools/idf.py', 'build'],
    env=env,
    capture_output=True,
    text=True,
    timeout=600
)

print(result.stdout[-3000:] if len(result.stdout) > 3000 else result.stdout)
print("Return code:", result.returncode)