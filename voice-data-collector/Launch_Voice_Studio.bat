@echo off
title TinyML Voice Studio
cd /d "%~dp0"
start "" pythonw gui_collector.py
exit
