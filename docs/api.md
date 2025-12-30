# Web API

This page documents the HTTP endpoints served by the ESP32.

Base URL:

- `http://<device-ip>/`

## UI

### `GET /`

Serves the single-page web UI (editor + knit mode).

## Files and patterns

Pattern files are stored in LittleFS under `/patterns/*.json`.

### `GET /api/files`

Returns a JSON array of pattern file paths.

**Response**
```json
["/patterns/default.json","/patterns/diamond.json"]
```

### `GET /api/pattern?file=<path>`

Loads a pattern file and returns the current pattern and active row.

If `file` is missing, the current configured pattern file is used.

**Response**
```json
{
  "file": "/patterns/diamond.json",
  "activeRow": 0,
  "pattern": {
    "name": "diamond",
    "w": 12,
    "h": 24,
    "pixels": ["000000000000", "..."]
  }
}
```

### `POST /api/pattern`

Saves a pattern file.

**Request**
```json
{
  "file": "/patterns/diamond.json",
  "pattern": {
    "name": "diamond",
    "w": 12,
    "h": 24,
    "pixels": ["0100...","..."]
  }
}
```

**Response**
```json
{"ok":true}
```

### `POST /api/delete`

Deletes a pattern file (except `/patterns/default.json`).

**Request**
```json
{"file":"/patterns/old.json"}
```

**Response**
```json
{"ok":true}
```

### `GET /download?file=<path>`

Downloads the selected pattern JSON as an attachment.

## Knitting controls

### `POST /api/row`

Steps the active row by `delta` (+1 or -1). The step respects:

- wrap-around at the ends
- row direction (`rowFromBottom`)

**Request**
```json
{"delta":1}
```

**Response**
```json
{"ok":true,"activeRow":3}
```

### `POST /api/confirm`

Marks the current row as confirmed. If `autoAdvance` is enabled, advances to next row (with wrap-around).

**Response**
```json
{"ok":true,"activeRow":4}
```

### `GET /api/state`

Polled by the web UI to stay in sync with hardware buttons/sensor.

**Response**
```json
{
  "activeRow": 3,
  "totalPulses": 53,
  "w": 12,
  "h": 24,
  "warn": false,
  "autoAdvance": true,
  "blinkWarning": true,
  "rowFromBottom": false,
  "brightness": 64,
  "colorActive": 65280,
  "colorConfirmed": 255
}
```

## Configuration

### `GET /api/config`

Returns current configuration.

### `POST /api/config`

Updates configuration. Any fields may be omitted; only provided values are updated.

**Request**
```json
{
  "colorActive": 65280,
  "colorConfirmed": 255,
  "brightness": 64,
  "autoAdvance": true,
  "blinkWarning": true,
  "rowFromBottom": false
}
```

**Response**
```json
{"ok":true}
```

## Upload

### `POST /upload`

Multipart upload. Uploaded file is stored under `/patterns/<filename>.json`.
