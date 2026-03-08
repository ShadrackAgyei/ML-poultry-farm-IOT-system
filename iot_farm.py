import json
from datetime import datetime
import mysql.connector
from mysql.connector import errorcode
import paho.mqtt.client as mqtt



DB_HOST = "localhost"      
DB_USER = "root"           
DB_PASSWORD = ""           
DB_NAME = "iot_farm"       


def connect_database():
    return mysql.connector.connect(
        host=DB_HOST,
        user=DB_USER,
        password=DB_PASSWORD,
        database=DB_NAME
    )



# Makes sure that the table exists
def ensure_table():
    db = connect_database()
    cursor = db.cursor()

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS readings (
            id INT AUTO_INCREMENT PRIMARY KEY,
            node INT,
            temp FLOAT,
            humidity FLOAT,
            light INT,
            timestamp DATETIME
        )
    """)

    db.commit()
    cursor.close()
    db.close()
    print(" Table 'readings' is ready!")


ensure_table()



#MQTT Broker Settings
MQTT_BROKER = "172.20.10.2"   # you may have to change to your broker IP or keep it as localhost
MQTT_PORT = 1883
MQTT_TOPIC = "farm/#"          # listens to all farm/nodeX topics


# Handles the incoming MQTT messages
def on_message(client, userdata, message):

    try:
        # decodes the message
        payload_str = message.payload.decode()
        print("\n MQTT Received:", payload_str)

        # converts JSON → dictionary
        data = json.loads(payload_str)

        node = data["node"]
        temp = data["temp"]
        humidity = data["humidity"]
        light = data["light"]
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        # connects to mysql
        db = connect_database()
        cursor = db.cursor()

        insert_query = """
            INSERT INTO readings (node, temp, humidity, light, timestamp)
            VALUES (%s, %s, %s, %s, %s)
        """

        values = (node, temp, humidity, light, timestamp)

        cursor.execute(insert_query, values)
        db.commit()

        cursor.close()
        db.close()

        print(f"Data saved → Node {node} | Temp={temp} | Time={timestamp}")

    except Exception as e:
        print(" Error:", e)



#When MQTT connects
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connection to MQTT Broker successful!")
        client.subscribe(MQTT_TOPIC)
        print("Subscribed to:", MQTT_TOPIC)
    else:
        print("MQTT connection has failed. Code:", rc)



#MAIN PROGRAM
def main():
    print("Connecting to MQTT Broker...")

    client = mqtt.Client()

    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(MQTT_BROKER, MQTT_PORT, 60)

    # Start listening forever
    client.loop_forever()


if __name__ == "__main__":
    main()
