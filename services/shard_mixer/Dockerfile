FROM python:3.11-slim

# Install dependencies – keep aligned with canon.yaml pins
RUN pip install --no-cache-dir fastapi==0.104.1 uvicorn[standard]==0.24.0 pydantic==2.5.2 \
    zstandard==0.22.0 numpy==1.26.4

WORKDIR /app
COPY main.py ./

# Data directory mount for SQLite & logs
VOLUME ["/data"]

EXPOSE 8000

CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"] 