package com.healthmonitor.app;

import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import java.util.List;

public class MainActivity extends AppCompatActivity implements NetworkService.DataCallback {

    private NetworkService networkService;
    private HealthRuleEngine ruleEngine;
    private TextView tvStatus, tvHR, tvSpO2, tvTemp, tvFall, tvFallUnit, tvStatusText, tvAiAdvice;
    private LinearLayout statusBadge, cardHR, cardSpO2, cardTemp, cardFall;
    private View statusDot;
    private HealthData latestData;
    private String serverHost = "192.168.4.1";
    private int serverPort = 5000;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        initViews();
        loadSettings();
        setupButtons();

        ruleEngine = new HealthRuleEngine(this);
        latestData = new HealthData();

        networkService = new NetworkService();
        networkService.setCallback(this);
        networkService.connect(serverHost, serverPort);
    }

    private void initViews() {
        tvStatus = findViewById(R.id.tvStatus);
        tvHR = findViewById(R.id.tvHR);
        tvSpO2 = findViewById(R.id.tvSpO2);
        tvTemp = findViewById(R.id.tvTemp);
        tvFall = findViewById(R.id.tvFall);
        tvFallUnit = findViewById(R.id.tvFallUnit);
        tvStatusText = findViewById(R.id.tvStatusText);
        tvAiAdvice = findViewById(R.id.tvAiAdvice);
        statusBadge = findViewById(R.id.statusBadge);
        statusDot = findViewById(R.id.statusDot);
        cardHR = findViewById(R.id.cardHR);
        cardSpO2 = findViewById(R.id.cardSpO2);
        cardTemp = findViewById(R.id.cardTemp);
        cardFall = findViewById(R.id.cardFall);
    }

    private void loadSettings() {
        SharedPreferences prefs = getSharedPreferences("health_monitor", MODE_PRIVATE);
        serverHost = prefs.getString("host", "192.168.4.1");
        serverPort = prefs.getInt("port", 5000);
    }

    private void setupButtons() {
        ImageButton btnSettings = findViewById(R.id.btnSettings);
        btnSettings.setOnClickListener(v -> {
            Intent intent = new Intent(this, SettingsActivity.class);
            intent.putExtra("host", serverHost);
            intent.putExtra("port", serverPort);
            startActivityForResult(intent, 100);
        });

        Button btnBreathStart = findViewById(R.id.btnBreathStart);
        Button btnBreathStop = findViewById(R.id.btnBreathStop);
        Button btnMute = findViewById(R.id.btnMute);
        Button btnConfirm = findViewById(R.id.btnConfirm);

        btnBreathStart.setOnClickListener(v -> networkService.sendCommand("breath_start"));
        btnBreathStop.setOnClickListener(v -> networkService.sendCommand("breath_stop"));
        btnMute.setOnClickListener(v -> networkService.sendCommand("mute"));
        btnConfirm.setOnClickListener(v -> networkService.sendCommand("confirm"));

        Button btnAiConsult = findViewById(R.id.btnAiConsult);
        btnAiConsult.setOnClickListener(v -> {
            Intent intent = new Intent(this, AiConsultActivity.class);
            intent.putExtra("health_data", latestData);
            startActivity(intent);
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == 100 && resultCode == RESULT_OK && data != null) {
            serverHost = data.getStringExtra("host");
            serverPort = data.getIntExtra("port", 5000);

            SharedPreferences.Editor editor = getSharedPreferences("health_monitor", MODE_PRIVATE).edit();
            editor.putString("host", serverHost);
            editor.putInt("port", serverPort);
            editor.apply();

            networkService.connect(serverHost, serverPort);
        }
    }

    @Override
    public void onDataReceived(HealthData data) {
        latestData = data;
        tvHR.setText(String.valueOf(data.getHeartRate()));
        tvSpO2.setText(String.valueOf(data.getSpo2()));
        tvTemp.setText(String.valueOf(data.getTemperature()));
        tvFall.setText(data.getFallConfidence() + "%");

        tvStatusText.setText(data.getStatus());

        updateCardAlert(cardHR, data.isHeartRateAbnormal(), tvHR);
        updateCardAlert(cardSpO2, data.isSpo2Abnormal(), tvSpO2);
        updateCardAlert(cardTemp, data.isTemperatureAbnormal(), tvTemp);
        updateCardAlert(cardFall, data.isFallDetected(), tvFall);

        if (data.isFallDetected()) {
            tvFallUnit.setText("⚠ 警报！");
            tvFallUnit.setTextColor(getColor(R.color.accent_red));
        } else {
            tvFallUnit.setText("置信度 %");
            tvFallUnit.setTextColor(getColor(R.color.text_secondary));
        }

        updateRuleEngineAdvice(data);
    }

    private void updateRuleEngineAdvice(HealthData data) {
        List<HealthRuleEngine.HealthAdvice> advices = ruleEngine.evaluate(data);
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < advices.size(); i++) {
            HealthRuleEngine.HealthAdvice a = advices.get(i);
            if (i > 0) sb.append("\n\n");
            sb.append(a.condition).append("：").append(a.advice);
        }
        tvAiAdvice.setText(sb.toString());
    }

    private void updateCardAlert(LinearLayout card, boolean isAlert, TextView valueView) {
        if (isAlert) {
            card.setBackgroundResource(R.drawable.bg_metric_card_alert);
            valueView.setTextColor(getColor(R.color.accent_red));
        } else {
            card.setBackgroundResource(R.drawable.bg_metric_card);
            valueView.setTextColor(getColor(R.color.text_primary));
        }
    }

    @Override
    public void onConnectionChanged(boolean connected) {
        if (connected) {
            statusBadge.setBackgroundResource(R.drawable.bg_status_badge_connected);
            statusDot.setBackgroundColor(getColor(R.color.accent_green));
            tvStatus.setText("设备已连接");
            tvStatus.setTextColor(getColor(R.color.accent_green));
        } else {
            statusBadge.setBackgroundResource(R.drawable.bg_status_badge);
            statusDot.setBackgroundColor(getColor(R.color.accent_red));
            tvStatus.setText("设备未连接");
            tvStatus.setTextColor(getColor(R.color.accent_red));
        }
    }

    @Override
    public void onError(String error) {
        Toast.makeText(this, error, Toast.LENGTH_SHORT).show();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (networkService != null) {
            networkService.disconnect();
        }
    }
}
