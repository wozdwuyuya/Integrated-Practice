package com.healthmonitor.app;

import android.content.Context;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

public class HealthRuleEngine {

    public enum Severity { NORMAL, INFO, WARNING, DANGER, EMERGENCY }

    public static class HealthAdvice {
        public final String category;
        public final String condition;
        public final String advice;
        public final Severity severity;

        public HealthAdvice(String category, String condition, String advice, Severity severity) {
            this.category = category;
            this.condition = condition;
            this.advice = advice;
            this.severity = severity;
        }
    }

    private final List<JSONObject> knowledgeBase = new ArrayList<>();

    public HealthRuleEngine(Context context) {
        loadKnowledgeBase(context);
    }

    private void loadKnowledgeBase(Context context) {
        try {
            InputStream is = context.getAssets().open("health_knowledge.json");
            byte[] buffer = new byte[is.available()];
            is.read(buffer);
            is.close();
            String json = new String(buffer, StandardCharsets.UTF_8);
            JSONArray arr = new JSONArray(json);
            for (int i = 0; i < arr.length(); i++) {
                knowledgeBase.add(arr.getJSONObject(i));
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public List<HealthAdvice> evaluate(HealthData data) {
        List<HealthAdvice> results = new ArrayList<>();
        double hr = data.getHeartRate();
        double spo2 = data.getSpo2();
        double temp = data.getTemperature();
        double[] accel = data.getAccel();
        double accelMag = Math.sqrt(accel[0] * accel[0] + accel[1] * accel[1] + accel[2] * accel[2]);

        try {
            for (JSONObject rule : knowledgeBase) {
                String threshold = rule.getString("threshold");
                String category = rule.getString("category");
                String condition = rule.getString("condition");
                String advice = rule.getString("advice");
                String sevStr = rule.getString("severity");
                Severity severity = parseSeverity(sevStr);

                boolean matched = false;
                switch (threshold) {
                    case "hr > 100": matched = hr > 100; break;
                    case "hr < 60": matched = hr < 60; break;
                    case "spo2 < 90": matched = spo2 < 90; break;
                    case "spo2 >= 90 and spo2 < 95": matched = spo2 >= 90 && spo2 < 95; break;
                    case "temp >= 37.3 and temp < 38.0": matched = temp >= 37.3 && temp < 38.0; break;
                    case "temp >= 38.0 and temp < 39.0": matched = temp >= 38.0 && temp < 39.0; break;
                    case "temp >= 39.0": matched = temp >= 39.0; break;
                    case "fall_conf > 50": matched = data.getFallConfidence() > 50; break;
                    case "accel_magnitude > 2.0": matched = accelMag > 2.0; break;
                    case "hr > 90 and temp > 37.0": matched = hr > 90 && temp > 37.0; break;
                    case "temp > 37.3 and spo2 >= 95": matched = temp > 37.3 && spo2 >= 95; break;
                    case "spo2 < 95 and hr > 100": matched = spo2 < 95 && hr > 100; break;
                    case "hr > 120": matched = hr > 120; break;
                    case "hr_variability_high":
                        break;
                    case "all_normal":
                        matched = hr >= 60 && hr <= 100 && spo2 >= 95 && temp < 37.3 && data.getFallConfidence() <= 50;
                        break;
                    case "user_request":
                        break;
                }

                if (matched) {
                    results.add(new HealthAdvice(category, condition, advice, severity));
                }
            }
        } catch (JSONException e) {
            e.printStackTrace();
        }

        if (results.isEmpty()) {
            results.add(new HealthAdvice("综合", "整体健康评估",
                    "当前各项指标均在正常范围内。保持健康生活方式：规律运动、充足睡眠、均衡饮食。",
                    Severity.NORMAL));
        }

        return results;
    }

    public List<HealthAdvice> searchByKeywords(String query) {
        List<HealthAdvice> results = new ArrayList<>();
        String lowerQuery = query.toLowerCase();

        try {
            for (JSONObject rule : knowledgeBase) {
                JSONArray keywords = rule.getJSONArray("keywords");
                boolean matched = false;
                for (int i = 0; i < keywords.length(); i++) {
                    if (lowerQuery.contains(keywords.getString(i).toLowerCase())) {
                        matched = true;
                        break;
                    }
                }
                if (!matched && lowerQuery.contains(rule.getString("category").toLowerCase())) {
                    matched = true;
                }
                if (matched) {
                    results.add(new HealthAdvice(
                            rule.getString("category"),
                            rule.getString("condition"),
                            rule.getString("advice"),
                            parseSeverity(rule.getString("severity"))
                    ));
                }
            }
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return results;
    }

    public String buildRagContext(HealthData data, String userQuery) {
        StringBuilder context = new StringBuilder();

        context.append("【当前健康数据】\n");
        context.append("心率：").append(data.getHeartRate()).append(" 次/分钟\n");
        context.append("血氧：").append(data.getSpo2()).append(" %\n");
        context.append("体温：").append(data.getTemperature()).append(" °C\n");
        context.append("跌倒置信度：").append(data.getFallConfidence()).append(" %\n");
        context.append("系统状态：").append(data.getStatus()).append("\n\n");

        context.append("【知识库匹配结果】\n");
        List<HealthAdvice> byData = evaluate(data);
        for (HealthAdvice a : byData) {
            context.append("- ").append(a.condition).append("：").append(a.advice).append("\n");
        }

        if (userQuery != null && !userQuery.isEmpty()) {
            List<HealthAdvice> byQuery = searchByKeywords(userQuery);
            if (!byQuery.isEmpty()) {
                context.append("\n【症状相关知识】\n");
                for (HealthAdvice a : byQuery) {
                    context.append("- ").append(a.condition).append("：").append(a.advice).append("\n");
                }
            }
        }

        return context.toString();
    }

    private Severity parseSeverity(String s) {
        switch (s) {
            case "info": return Severity.INFO;
            case "warning": return Severity.WARNING;
            case "danger": return Severity.DANGER;
            case "emergency": return Severity.EMERGENCY;
            default: return Severity.NORMAL;
        }
    }
}
