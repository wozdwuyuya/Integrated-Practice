package com.healthmonitor.app;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class NetworkService {
    private static final String TAG = "NetworkService";
    private static final int CONNECT_TIMEOUT = 5000;
    private static final int READ_TIMEOUT = 10000;
    private static final int RECONNECT_DELAY = 5000;

    public interface DataCallback {
        void onDataReceived(HealthData data);
        void onConnectionChanged(boolean connected);
        void onError(String error);
    }

    private Socket socket;
    private boolean connected = false;
    private boolean shouldReconnect = false;
    private String host;
    private int port;
    private DataCallback callback;
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private Thread readThread;

    public void setCallback(DataCallback callback) {
        this.callback = callback;
    }

    public boolean isConnected() {
        return connected;
    }

    public void connect(String host, int port) {
        this.host = host;
        this.port = port;
        this.shouldReconnect = true;
        disconnect();
        executor.execute(this::connectInternal);
    }

    private void connectInternal() {
        try {
            Log.d(TAG, "正在连接 " + host + ":" + port);
            socket = new Socket();
            socket.connect(new InetSocketAddress(host, port), CONNECT_TIMEOUT);
            socket.setSoTimeout(READ_TIMEOUT);
            socket.setKeepAlive(true);

            connected = true;
            notifyConnectionChanged(true);
            Log.d(TAG, "连接成功");

            startReading();
        } catch (Exception e) {
            Log.e(TAG, "连接失败: " + e.getMessage());
            connected = false;
            notifyConnectionChanged(false);
            notifyError("无法连接: " + e.getMessage());
            if (shouldReconnect) {
                scheduleReconnect();
            }
        }
    }

    private void startReading() {
        readThread = new Thread(() -> {
            try {
                BufferedReader reader = new BufferedReader(
                        new InputStreamReader(socket.getInputStream()));
                String line;
                while (connected && (line = reader.readLine()) != null) {
                    line = line.trim();
                    if (line.isEmpty()) continue;
                    parseAndNotify(line);
                }
            } catch (Exception e) {
                if (connected) {
                    Log.e(TAG, "读取错误: " + e.getMessage());
                }
            } finally {
                handleDisconnect();
            }
        });
        readThread.setDaemon(true);
        readThread.start();
    }

    private void parseAndNotify(String line) {
        try {
            JSONObject json = new JSONObject(line);
            if (json.has("hr") || json.has("spo2")) {
                HealthData data = HealthData.fromJson(json);
                notifyDataReceived(data);
            }
        } catch (JSONException e) {
            Log.w(TAG, "JSON解析失败: " + line);
        }
    }

    private void handleDisconnect() {
        connected = false;
        notifyConnectionChanged(false);
        if (shouldReconnect) {
            scheduleReconnect();
        }
    }

    private void scheduleReconnect() {
        mainHandler.postDelayed(() -> {
            if (shouldReconnect && !connected) {
                Log.d(TAG, "尝试重新连接...");
                executor.execute(this::connectInternal);
            }
        }, RECONNECT_DELAY);
    }

    public void sendCommand(String command) {
        if (!connected || socket == null) return;
        executor.execute(() -> {
            try {
                JSONObject cmd = new JSONObject();
                cmd.put("command", command);
                String payload = cmd.toString() + "\n";
                OutputStream out = socket.getOutputStream();
                out.write(payload.getBytes());
                out.flush();
                Log.d(TAG, "已发送命令: " + command);
            } catch (Exception e) {
                Log.e(TAG, "发送失败: " + e.getMessage());
            }
        });
    }

    public void disconnect() {
        shouldReconnect = false;
        connected = false;
        try {
            if (socket != null && !socket.isClosed()) {
                socket.close();
            }
        } catch (Exception e) {
            Log.e(TAG, "断开连接错误: " + e.getMessage());
        }
        socket = null;
        notifyConnectionChanged(false);
    }

    private void notifyDataReceived(HealthData data) {
        if (callback != null) {
            mainHandler.post(() -> callback.onDataReceived(data));
        }
    }

    private void notifyConnectionChanged(boolean isConnected) {
        if (callback != null) {
            mainHandler.post(() -> callback.onConnectionChanged(isConnected));
        }
    }

    private void notifyError(String error) {
        if (callback != null) {
            mainHandler.post(() -> callback.onError(error));
        }
    }
}
