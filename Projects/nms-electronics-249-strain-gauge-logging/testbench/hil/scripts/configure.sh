#!/bin/bash
set -e
echo -e "\n\nInstalling Linux Utils\n\n"
sudo apt update 
sudo apt install git can-utils net-tools i2c-tools

echo -e "\n\nInstalling uv\n\n" 
curl -LsSf https://astral.sh/uv/install.sh | sh

# create gpio group and add yourself to it
getent group gpio || sudo groupadd gpio
sudo usermod -aG gpio $USER

# allow anyone in the gpio group to access the gpios
# without needing to be root
echo "Creating persistent udev rule for /dev/gpiomem ..."
# create the udev rule
sudo tee /etc/udev/rules.d/99-gpiomem-perms.rules <<'EOF'
# Give gpio group access to gpiomem
KERNEL=="gpiomem", GROUP="gpio", MODE="0660"
EOF
# reload udev and apply the rule immediately
sudo udevadm control --reload-rules
sudo udevadm trigger
