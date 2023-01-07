from flask import Flask, render_template
from gevent import pywsgi

app = Flask(__name__)
app.debug = True


@app.route('/hello', methods=['GET'])
def hello_world():
    return render_template('hello.html')


if __name__ == '__main__':
    app.run()
    # server = pywsgi.WSGIServer(('0.0.0.0', 5000), app)
    # server.serve_forever()
