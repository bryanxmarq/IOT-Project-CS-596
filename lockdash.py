from flask import Flask, request, render_template_string
from datetime import datetime, timedelta
from collections import defaultdict

app = Flask(__name__)

# in-memory logs and counters
attempt_log = []
fail_counter = defaultdict(int)
lockout_time = defaultdict(datetime)  # track lockout time per code

# suspicious code patterns
suspicious_patterns = {"0000", "1111", "8888"}

# lockout parameters
MAX_ATTEMPTS = 3
LOCKOUT_DURATION = timedelta(minutes=1)  # Lock out for 1 minute

@app.route("/", methods=["GET"])
def handle_request():
    status = request.args.get("status")
    code = request.args.get("code")
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    now = datetime.now()

    print(f"[{timestamp}] Status: {status}, Code: {code}")

    response_note = ""

    # check if code is locked out
    if status == "attempt":
        # check if its currently locked out
        if code in lockout_time and now < lockout_time[code]:
            wait_seconds = int((lockout_time[code] - now).total_seconds())
            response_note = f"LOCKED OUT for {wait_seconds} seconds due to repeated failures."
            print(response_note)
            return f"Access denied: {response_note}"

        fail_counter[code] += 1

        #if max attempt reached apply lockout
        if fail_counter[code] >= MAX_ATTEMPTS:
            lockout_time[code] = now + LOCKOUT_DURATION
            response_note = f"Code {code} has been LOCKED OUT for {LOCKOUT_DURATION.total_seconds()} seconds"
            print(response_note)

        # flag suspicious pattern
        elif code in suspicious_patterns:
            response_note = f"Suspicious pattern detected: {code}"
            print(response_note)

    elif status == "unlocked":
        # reset on successful unlock
        fail_counter[code] = 0
        lockout_time.pop(code, None)

    # log attempt
    attempt_log.append({
        "timestamp": timestamp,
        "status": status,
        "code": code,
        "note": response_note
    })

return f"Received: Status={status}, Code={code} at {timestamp}. {response_note}"


@app.route("/dashboard")
def dashboard():
    html_template = """
    <html>
    <head>
        <title>Smart Lock Dashboard</title>
        <style>
            body { font-family: Arial, sans-serif; background-color: #f4f4f4; padding: 20px; }
            h1 { color: #333; }
            table { width: 100%; border-collapse: collapse; margin-top: 20px; }
            th, td { border: 1px solid #ccc; padding: 10px; text-align: left; }
            th { background-color: #eee; }
            tr:nth-child(even) { background-color: #fafafa; }
            .note { color: red; font-weight: bold; }
        </style>
    </head>
    <body>
        <h1>Smart Lock Attempt Log</h1>
        <table>
            <tr>
                <th>Timestamp</th>
                <th>Status</th>
                <th>Code</th>
                <th>Note</th>
            </tr>
            {% for entry in log %}
            <tr>
                <td>{{ entry.timestamp }}</td>
                <td>{{ entry.status }}</td>
                <td>{{ entry.code }}</td>
                <td class="note">{{ entry.note }}</td>
            </tr>
            {% endfor %}
        </table>
    </body>
    </html>
    """
    return render_template_string(html_template, log=attempt_log)
    
# run flask server
if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)

