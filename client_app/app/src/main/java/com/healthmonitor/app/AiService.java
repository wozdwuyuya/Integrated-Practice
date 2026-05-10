package com.healthmonitor.app;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class AiService {
    private static final String TAG = "AiService";
    private static final int HTTP_PORT = 5000;
    private static final String SYSTEM_PROMPT =
            "你是一个专业的智能健康助手，集成在智能健康监测系统中。" +
            "你会根据用户的实时健康数据（心率、血氧、体温、跌倒检测等）提供专业的健康分析和建议。" +
            "请用中文回答，语言亲切专业。" +
            "重要提醒：你的建议仅供参考，不能替代专业医疗诊断。如有严重症状请建议用户及时就医。";

    public interface AiCallback {
        void onSuccess(String response);
        void onError(String error);
        void onLoading(boolean loading);
    }

    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final List<JSONObject> conversationHistory = new ArrayList<>();
    private String serverUrl;

    public AiService(String serverHost) {
        this.serverUrl = "http://" + serverHost + ":" + HTTP_PORT;
    }

    public void setServerHost(String serverHost) {
        this.serverUrl = "http://" + serverHost + ":" + HTTP_PORT;
    }

    public void clearHistory() {
        conversationHistory.clear();
    }

    public void ask(String ragContext, String userMessage, AiCallback callback) {
        mainHandler.post(() -> callback.onLoading(true));

        executor.execute(() -> {
            try {
                JSONObject systemMsg = new JSONObject();
                systemMsg.put("role", "system");
                systemMsg.put("content", SYSTEM_PROMPT + "\n\n" + ragContext);

                JSONObject userMsg = new JSONObject();
                userMsg.put("role", "user");
                userMsg.put("content", userMessage);

                JSONArray messages = new JSONArray();
                messages.put(systemMsg);
                for (JSONObject msg : conversationHistory) {
                    messages.put(msg);
                }
                messages.put(userMsg);

                JSONObject requestBody = new JSONObject();
                requestBody.put("messages", messages);

                String response = httpPost(serverUrl + "/api/ai/chat", requestBody.toString());

                JSONObject jsonResponse = new JSONObject(response);
                if (jsonResponse.has("error")) {
                    String error = jsonResponse.getString("error");
                    mainHandler.post(() -> {
                        callback.onLoading(false);
                        callback.onError(error);
                    });
                    return;
                }

                String aiReply = jsonResponse.optString("reply", "抱歉，AI 暂时无法回答。");

                JSONObject assistantMsg = new JSONObject();
                assistantMsg.put("role", "assistant");
                assistantMsg.put("content", aiReply);
                conversationHistory.add(userMsg);
                conversationHistory.add(assistantMsg);

                if (conversationHistory.size() > 20) {
                    conversationHistory.subList(0, conversationHistory.size() - 20).clear();
                }

                mainHandler.post(() -> {
                    callback.onLoading(false);
                    callback.onSuccess(aiReply);
                });

            } catch (Exception e) {
                Log.e(TAG, "AI请求失败", e);
                mainHandler.post(() -> {
                    callback.onLoading(false);
                    callback.onError("请求失败: " + e.getMessage());
                });
            }
        });
    }

    private String httpPost(String urlStr, String jsonBody) throws Exception {
        URL url = new URL(urlStr);
        HttpURLConnection conn = (HttpURLConnection) url.openConnection();
        conn.setRequestMethod("POST");
        conn.setRequestProperty("Content-Type", "application/json; charset=utf-8");
        conn.setConnectTimeout(15000);
        conn.setReadTimeout(60000);
        conn.setDoOutput(true);

        try (OutputStream os = conn.getOutputStream()) {
            os.write(jsonBody.getBytes(StandardCharsets.UTF_8));
            os.flush();
        }

        int code = conn.getResponseCode();
        BufferedReader reader;
        if (code >= 200 && code < 300) {
            reader = new BufferedReader(new InputStreamReader(conn.getInputStream(), StandardCharsets.UTF_8));
        } else {
            reader = new BufferedReader(new InputStreamReader(conn.getErrorStream(), StandardCharsets.UTF_8));
        }

        StringBuilder sb = new StringBuilder();
        String line;
        while ((line = reader.readLine()) != null) {
            sb.append(line);
        }
        reader.close();
        conn.disconnect();

        if (code < 200 || code >= 300) {
            throw new Exception("HTTP " + code + ": " + sb.toString());
        }

        return sb.toString();
    }
}
