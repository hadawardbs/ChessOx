#!/bin/bash
echo "Setting up Oxta Environment..."
pip install -r requirements.txt
python3 build.py
echo "Setup Complete."
