package com.healthmonitor.app;

import android.os.Parcel;
import android.os.Parcelable;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

public class HealthData implements Parcelable {
    private int heartRate;
    private double spo2;
    private double temperature;
    private double[] accel;
    private double[] gyro;
    private int fallConfidence;
    private String status;
    private long timestamp;

    public HealthData() {
        this.heartRate = 0;
        this.spo2 = 0;
        this.temperature = 0;
        this.accel = new double[]{0, 0, 0};
        this.gyro = new double[]{0, 0, 0};
        this.fallConfidence = 0;
        this.status = "状态：--";
        this.timestamp = System.currentTimeMillis();
    }

    public static HealthData fromJson(JSONObject json) {
        HealthData data = new HealthData();
        try {
            if (json.has("hr")) data.heartRate = json.getInt("hr");
            if (json.has("spo2")) data.spo2 = json.getDouble("spo2");
            if (json.has("temp")) data.temperature = json.getDouble("temp");
            if (json.has("fall_conf")) data.fallConfidence = json.getInt("fall_conf");
            if (json.has("status")) data.status = json.getString("status");

            if (json.has("accel")) {
                JSONArray arr = json.getJSONArray("accel");
                data.accel = new double[arr.length()];
                for (int i = 0; i < arr.length(); i++) {
                    data.accel[i] = arr.getDouble(i);
                }
            }
            if (json.has("gyro")) {
                JSONArray arr = json.getJSONArray("gyro");
                data.gyro = new double[arr.length()];
                for (int i = 0; i < arr.length(); i++) {
                    data.gyro[i] = arr.getDouble(i);
                }
            }
        } catch (JSONException e) {
            e.printStackTrace();
        }
        data.timestamp = System.currentTimeMillis();
        return data;
    }

    public String toJsonString() {
        try {
            JSONObject json = new JSONObject();
            json.put("type", "data");
            json.put("hr", heartRate);
            json.put("spo2", spo2);
            json.put("temp", temperature);
            json.put("fall_conf", fallConfidence);
            json.put("status", status);

            JSONArray accelArr = new JSONArray();
            for (double v : accel) accelArr.put(v);
            json.put("accel", accelArr);

            JSONArray gyroArr = new JSONArray();
            for (double v : gyro) gyroArr.put(v);
            json.put("gyro", gyroArr);

            return json.toString();
        } catch (JSONException e) {
            return "{}";
        }
    }

    public int getHeartRate() { return heartRate; }
    public double getSpo2() { return spo2; }
    public double getTemperature() { return temperature; }
    public double[] getAccel() { return accel; }
    public double[] getGyro() { return gyro; }
    public int getFallConfidence() { return fallConfidence; }
    public String getStatus() { return status; }
    public long getTimestamp() { return timestamp; }

    public boolean isHeartRateAbnormal() { return heartRate > 100 || heartRate < 60; }
    public boolean isSpo2Abnormal() { return spo2 < 90; }
    public boolean isTemperatureAbnormal() { return temperature > 37.5; }
    public boolean isFallDetected() { return fallConfidence > 50; }

    protected HealthData(Parcel in) {
        heartRate = in.readInt();
        spo2 = in.readDouble();
        temperature = in.readDouble();
        accel = new double[]{in.readDouble(), in.readDouble(), in.readDouble()};
        gyro = new double[]{in.readDouble(), in.readDouble(), in.readDouble()};
        fallConfidence = in.readInt();
        status = in.readString();
        timestamp = in.readLong();
    }

    public static final Creator<HealthData> CREATOR = new Creator<HealthData>() {
        @Override
        public HealthData createFromParcel(Parcel in) {
            return new HealthData(in);
        }

        @Override
        public HealthData[] newArray(int size) {
            return new HealthData[size];
        }
    };

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeInt(heartRate);
        dest.writeDouble(spo2);
        dest.writeDouble(temperature);
        for (double v : accel) dest.writeDouble(v);
        for (double v : gyro) dest.writeDouble(v);
        dest.writeInt(fallConfidence);
        dest.writeString(status);
        dest.writeLong(timestamp);
    }
}
