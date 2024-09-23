import boto3
from botocore.exceptions import NoCredentialsError, PartialCredentialsError
import os
import json
from flask import Flask, jsonify, request
from ultralytics import YOLO  # Make sure you have installed the YOLO library
from PIL import Image
import requests
from io import BytesIO

# AWS Configuration
aws_access_key_id = 'AWS_ACCESS-KEY'
aws_secret_access_key = 'AWS_SECRET_ACCESS'
region_name = 'REGION'
bucket_name = 'BUCKET_NAME'
model_key = 'model/best.pt'  

s3 = boto3.client('s3', aws_access_key_id=aws_access_key_id, aws_secret_access_key=aws_secret_access_key, region_name=region_name)
s3.download_file(bucket_name, model_key, 'aws/model/best.pt')

app = Flask(__name__)

def get_latest_image_filename(s3_client, bucket):
    try:
        # List objects in the bucket
        response = s3_client.list_objects_v2(Bucket=bucket)

        if 'Contents' not in response:
            print("No images found in the bucket.")
            return None

        # Filter objects to keep only images
        image_extensions = ['.jpg', '.jpeg', '.png', '.gif', '.bmp', '.tiff']
        image_objects = [obj for obj in response['Contents'] if any(obj['Key'].lower().endswith(ext) for ext in image_extensions)]

        if not image_objects:
            print("No images found in the bucket.")
            return None

        # Sort objects by last modified date
        sorted_images = sorted(image_objects, key=lambda obj: obj['LastModified'], reverse=True)

        # Get the latest object
        latest_image = sorted_images[0]
        latest_image_key = latest_image['Key']

        return latest_image_key

    except Exception as e:
        print(f"An error occurred: {e}")
        return None

model = YOLO('aws/model/best.pt')

@app.route('/predict', methods=['GET'])
def predict():
    try:
        s3 = boto3.client(
            's3',
            aws_access_key_id=aws_access_key_id,
            aws_secret_access_key=aws_secret_access_key,
            region_name=region_name
        )

        # Get the filename of the latest image
        image_filename = get_latest_image_filename(s3, bucket_name)

        if image_filename is None:
            return jsonify({"error": "No images found"}), 404

        # Download the image
        image_obj = s3.get_object(Bucket=bucket_name, Key=image_filename)
        img = Image.open(BytesIO(image_obj['Body'].read()))

        # Make a prediction on the image
        results = model.predict(img)
        predictions = []
        for r in results:
            classes = r.boxes.cls.tolist()
            predictions.append(classes)

        # Determine prediction result based on conditions
        if not predictions or predictions == [[]] or any(cls == 0 for cls_list in predictions for cls in cls_list):
            prediction = 0
        else:
            prediction = 1

        # Create the result JSON
        result = {
            "prediction": prediction,
            "image_filename": image_filename
        }

        # Save the result JSON to S3
        result_json = json.dumps(result)
        result_key = f'predict/{os.path.splitext(os.path.basename(image_filename))[0]}_predictions.json'
        s3.put_object(Bucket=bucket_name, Key=result_key, Body=result_json, ContentType='application/json')

        return jsonify(result)
    
    except NoCredentialsError:
        return jsonify({"error": "Credentials not available"}), 403
    except PartialCredentialsError:
        return jsonify({"error": "Incomplete credentials provided"}), 403
    except Exception as e:
        return jsonify({"error": str(e)}), 500

if __name__ == "__main__":
    app.run(host='10.33.5.217', port=5000)
