@echo off
echo Setting up Oxta Environment...
pip install -r requirements.txt
python build.py
echo Setup Complete.
pause
