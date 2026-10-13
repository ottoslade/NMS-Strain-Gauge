# Install script to install all dependencies Linux machine will need

import subprocess
from getpass import getpass

def installs():
    try:
        password = getpass(prompt="Enter sudo password: ")
    except:
        return
    try:
        subprocess.check_call(['echo', password, '|', 'sudo', 'apt-get', 'install', '-y', 'gpio'], shell=True)
        print('Successfully installed gpio package')
    except subprocess.CalledProcessError as e:
        print(f'Failed to install gpio package. Error: {e}')

if __name__ == '__main__':
    installs()