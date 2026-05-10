package com.healthmonitor.app;

import android.content.Intent;
import android.os.Bundle;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

public class SettingsActivity extends AppCompatActivity {

    private EditText etHost, etPort;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_settings);

        etHost = findViewById(R.id.etHost);
        etPort = findViewById(R.id.etPort);
        Button btnSave = findViewById(R.id.btnSave);
        Button btnBack = findViewById(R.id.btnBack);

        String host = getIntent().getStringExtra("host");
        int port = getIntent().getIntExtra("port", 5000);
        etHost.setText(host);
        etPort.setText(String.valueOf(port));

        btnSave.setOnClickListener(v -> {
            String newHost = etHost.getText().toString().trim();
            String portStr = etPort.getText().toString().trim();

            if (newHost.isEmpty()) {
                Toast.makeText(this, "请输入服务器地址", Toast.LENGTH_SHORT).show();
                return;
            }

            int newPort;
            try {
                newPort = Integer.parseInt(portStr);
            } catch (NumberFormatException e) {
                Toast.makeText(this, "端口号格式错误", Toast.LENGTH_SHORT).show();
                return;
            }

            Intent result = new Intent();
            result.putExtra("host", newHost);
            result.putExtra("port", newPort);
            setResult(RESULT_OK, result);

            Toast.makeText(this, "正在连接 " + newHost + ":" + newPort, Toast.LENGTH_SHORT).show();
            finish();
        });

        btnBack.setOnClickListener(v -> finish());
    }
}
