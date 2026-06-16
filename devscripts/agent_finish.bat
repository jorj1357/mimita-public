@echo off
REM Agent task completion wrapper
REM Usage: agent_finish.bat [task_name]

python devscripts\agent_task_complete.py %*
