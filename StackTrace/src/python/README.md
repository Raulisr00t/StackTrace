# stacktrace
Low-level Windows Stack Trace Engine — walks the stack of any running process without DbgHelp.

## Install
pip install stacktrace

## Usage
```python
import stacktrace
result = stacktrace.trace(pid=1234)
for frame in result.frames:
    print(frame)
```