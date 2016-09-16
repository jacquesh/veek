from flask import Flask, Response, request, redirect, send_from_directory, abort
from werkzeug.utils import secure_filename

from datetime import datetime
from os import listdir, mkdir, remove
from os.path import isdir, isfile, join

app = Flask(__name__)
app.config["MAX_CONTENT_LENGTH"] = 500 * 1024
app.config["LOG_DIR"] = "logs"
app.config["BIN_DIR"] = "bin"

@app.route("/", methods=["POST"])
def logUpload():
    if "username" in request.headers:
        name = request.headers["username"]
        dateStr = datetime.today().strftime("%d-%m-%Y_%H-%M-%S")
        filename = secure_filename("%s_%s.log" % (name, dateStr))
        filepath = join(app.config["LOG_DIR"], filename)
        out = open(filepath, "wb")
        out.write(request.data)
        out.close()
    return ""

@app.route("/", methods=["GET"])
def logListView():
    logList = listdir(app.config["LOG_DIR"])
    if len(logList) == 0:
        return "There are no logs to view"
    logList.sort()

    result = ""
    result += "<style> table {border-collapse:collapse;} td {padding-right:20px; border:none} tr:nth-child(even) {background-color: #f2f2f2} </style>"
    result += "<table>"
    for filename in logList:
        title = filename[:filename.rfind(".")]
        viewLink = "<a href='/view/%s'>View</a>" % title
        downloadLink = "<a href='/download/%s'>Download</a>" % title
        deleteLink = "<a href='/delete/%s'>Delete</a>" % title
        result += "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % (title, viewLink, downloadLink, deleteLink)
    result += "</table>"
    return result

@app.route("/view/<logname>")
def logView(logname):
    logFileName = logname+".log"
    logFilePath = join(app.config["LOG_DIR"], logFileName)
    logFile = open(logFilePath, "r")
    logStr = logFile.read()
    logFile.close()
    logStr = logStr.replace("\r\n", "\n")
    logStr = logStr.replace("\n", "<br/>")
    logStr = logStr.replace(" ", "&nbsp;")
    backLink = "<p><a href='/'>Back</a><p/>"
    logStr = "%s%s%s" % (backLink, logStr, backLink)
    return logStr

@app.route("/delete/<logname>")
def logDelete(logname):
    logFileName = secure_filename("%s.log" % logname)
    logFilePath = join(app.config["LOG_DIR"], logFileName)
    if(isfile(logFilePath)):
        remove(logFilePath)
        return redirect("/")
    else:
        abort(404)


@app.route("/download/<logname>")
def logDownload(logname):
    return send_from_directory(app.config["LOG_DIR"], logname+".log", as_attachment=True, attachment_filename=logname+".log")

@app.route("/get")
def appDownload():
    return send_from_directory(app.config["BIN_DIR"], "veek.zip", as_attachment=True, attachment_filename="veek.zip")

if __name__ == "__main__":
    if not isdir(app.config["LOG_DIR"]):
        mkdir(app.config["LOG_DIR"])
    if not isdir(app.config["BIN_DIR"]):
        mkdir(app.config["BIN_DIR"])
    app.run(host="0.0.0.0", port=80)
