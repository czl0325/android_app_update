package com.github.app_update_client;

import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v4.content.FileProvider;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

import java.io.File;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Log.e("czl", myTestString());

        int permissionCheck1 = ContextCompat.checkSelfPermission(getApplicationContext(), Manifest.permission.READ_EXTERNAL_STORAGE);
        int permissionCheck2 = ContextCompat.checkSelfPermission(getApplicationContext(), Manifest.permission.WRITE_EXTERNAL_STORAGE);
        if (permissionCheck1 != PackageManager.PERMISSION_GRANTED || permissionCheck2 != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE, Manifest.permission.READ_EXTERNAL_STORAGE},
                    124);
        } else {
            startPatch();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           String[] permissions,
                                           int[] grantResults) {
        if (requestCode == 124) {
            if ((grantResults.length > 0) && (grantResults[0] == PackageManager.PERMISSION_GRANTED)) {
                startPatch();
            } else {
                Log.d("czl","用户拒绝");
            }
        }
    }

    private void startPatch() {
        if (Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED)) {
            //File path = new File(Environment.getExternalStorageDirectory().getAbsolutePath()+File.separator+"Download");
            String oldFile = Environment.getExternalStorageDirectory().getAbsolutePath()+File.separator
                    +"Download"+File.separator+"weixin_v6.0.apk";
            String newFile = this.getFilesDir()+File.separator+"weixin_v6.6.apk";
            String patchFile = Environment.getExternalStorageDirectory().getAbsolutePath()+File.separator
                    +"Download"+File.separator+"weixin.patch";
            if (new File(oldFile).exists() && new File(patchFile).exists()) {
                int result = patch(oldFile, newFile, patchFile);
                Log.e("czl",""+result);
                File newF = new File(newFile);
                if (newF.exists()) {
                    Intent intent = new Intent(Intent.ACTION_VIEW);
                    intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                        Log.w("czl", "版本大于 N ，开始使用 fileProvider 进行安装");
                        intent.setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                        Uri contentUri = FileProvider.getUriForFile(
                                this
                                , "com.github.app_update_client.fileprovider"
                                , newF);
                        intent.setDataAndType(contentUri, "application/vnd.android.package-archive");
                    } else {
                        Log.w("czl", "正常进行安装");
                        intent.setDataAndType(Uri.fromFile(newF), "application/vnd.android.package-archive");
                    }
                    startActivity(intent);
                }
            }
        }
    }

    public native static String stringFromJNI();
    public native static int patch(String oldFile, String newFile, String patchFile);
    public native static String myTestString();
}
