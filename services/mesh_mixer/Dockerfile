FROM python:3.11-slim

# Authoritative constants live in canon.yaml; install runtime deps only.
RUN pip install --no-cache-dir fastapi==0.104.1 uvicorn[standard]==0.24.0 pydantic==2.5.2

WORKDIR /app
COPY main.py ./

# Data directory mount (define via docker-compose)
VOLUME ["/data"]

EXPOSE 8000

CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"] 