package com.alexbatalov.falloutce;

import android.app.Activity;
import android.app.ProgressDialog;
import android.content.ContentResolver;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import androidx.documentfile.provider.DocumentFile;

import java.io.File;

public class ImportActivity extends Activity {
    private static final int IMPORT_REQUEST_CODE = 1;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        startActivityForResult(intent, IMPORT_REQUEST_CODE);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent resultData) {
        if (requestCode == IMPORT_REQUEST_CODE) {
            if (resultCode == Activity.RESULT_OK) {
                final Uri treeUri = resultData.getData();
                if (treeUri != null) {
                    final DocumentFile treeDocument = DocumentFile.fromTreeUri(this, treeUri);
                    if (treeDocument != null) {
                        copyFiles(treeDocument);
                        return;
                    }
                }
            }

            finish();
        } else {
            super.onActivityResult(requestCode, resultCode, resultData);
        }
    }

    private void copyFiles(DocumentFile treeDocument) {
        ProgressDialog dialog = createProgressDialog();
        dialog.show();

        new Thread(() -> {
            ContentResolver contentResolver = getContentResolver();
            File externalFilesDir = getExternalFilesDir(null);
            FileUtils.copyRecursively(contentResolver, treeDocument, externalFilesDir);

            startMainActivity();
            dialog.dismiss();
            finish();
        }).start();
    }

    private void startMainActivity() {
        Intent intent = new Intent(this, MainActivity.class);
        startActivity(intent);
    }

    private ProgressDialog createProgressDialog() {
        ProgressDialog progressDialog = new ProgressDialog(this,
            android.R.style.Theme_Material_Light_Dialog);
        progressDialog.setTitle(R.string.loading_dialog_title);
        progressDialog.setMessage(getString(R.string.loading_dialog_message));
        progressDialog.setProgressStyle(ProgressDialog.STYLE_SPINNER);
        progressDialog.setCancelable(false);

        return progressDialog;
    }
}
