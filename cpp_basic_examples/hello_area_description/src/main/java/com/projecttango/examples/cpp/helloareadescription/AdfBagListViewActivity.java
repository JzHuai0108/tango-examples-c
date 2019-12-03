
package com.projecttango.examples.cpp.helloareadescription;

import android.app.Activity;
import android.app.FragmentManager;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.Environment;
import android.os.IBinder;
import android.util.Log;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.Toast;

import com.projecttango.examples.cpp.util.TangoInitializationHelper;

import java.io.File;
import java.util.ArrayList;
import java.util.StringTokenizer;

/**
 * Creates a ListView to manage ADF Sessions.
 * Showcases:
 * - Exporting the ADF Bags of one session to raw data.
 * - Deleting an ADF session.
 * - Deleting the exported data for an ADF session.
 */
public class AdfBagListViewActivity extends Activity {
    private static final String TAG = AdfBagListViewActivity.class.getSimpleName();
    private String mTangoBagOutputDir;
    private static final int TANGO_INTENT_ACTIVITY_CODE = 3;

    // Tango service connection.
    ServiceConnection mTangoServiceConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            TangoJniNative.onTangoServiceConnected(service, false, false);
            updateList();
        }

        @Override
        public void onServiceDisconnected(ComponentName componentName) {
            // Handle this if you need to gracefully shutdown/retry
            // in the event that Tango itself crashes/gets upgraded while running.
        }
    };

    private ListView mTangoSpaceAdfListView, mAppSpaceAdfListView;
    private AdfUuidArrayAdapter mOriginalAdfListAdapter, mExportedAdfListAdapter;
    private ArrayList<AdfData> mOriginalAdfDataList, mExportedAdfDataList;
    private String[] mTangoSpaceMenuStrings, mAppSpaceMenuStrings;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.adf_bag_list_view);
        Intent intent = getIntent();
        mTangoBagOutputDir = intent.getStringExtra(StartActivity.INTENT_TANGO_BAG_OUTPUT_DIR);

        mTangoSpaceMenuStrings = getResources().getStringArray(
                R.array.set_dialog_menu_items_bag);
        mAppSpaceMenuStrings = getResources().getStringArray(
                R.array.set_dialog_menu_items_exported_data);

        // Get API ADF ListView Ready
        mTangoSpaceAdfListView = (ListView) findViewById(R.id.uuid_list_view_bags);
        mOriginalAdfDataList = new ArrayList<AdfData>();
        mOriginalAdfListAdapter = new AdfUuidArrayAdapter(this, mOriginalAdfDataList);
        mTangoSpaceAdfListView.setAdapter(mOriginalAdfListAdapter);
        registerForContextMenu(mTangoSpaceAdfListView);

        // Get App Space ADF List View Ready
        mAppSpaceAdfListView = (ListView) findViewById(R.id.uuid_list_view_exported_bags);

        mExportedAdfDataList = new ArrayList<AdfData>();
        mExportedAdfListAdapter = new AdfUuidArrayAdapter(this, mExportedAdfDataList);
        mAppSpaceAdfListView.setAdapter(mExportedAdfListAdapter);
        registerForContextMenu(mAppSpaceAdfListView);
    }

    @Override
    protected void onResume() {
        super.onResume();
        TangoInitializationHelper.bindTangoService(this, mTangoServiceConnection);
    }

    @Override
    protected void onPause() {
        super.onPause();
        TangoJniNative.onPause();
        unbindService(mTangoServiceConnection);
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) menuInfo;
        if (v.getId() == R.id.uuid_list_view_bags) {
            menu.setHeaderTitle(mOriginalAdfDataList.get(info.position).uuid);
            menu.add(mTangoSpaceMenuStrings[0]);
            menu.add(mTangoSpaceMenuStrings[1]);
        }

        if (v.getId() == R.id.uuid_list_view_exported_bags) {
            menu.setHeaderTitle(mExportedAdfDataList.get(info.position).uuid);
            menu.add(mAppSpaceMenuStrings[0]);
        }
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        AdapterView.AdapterContextMenuInfo info =
                (AdapterView.AdapterContextMenuInfo) item.getMenuInfo();
        String itemName = (String) item.getTitle();
        int index = info.position;

        if (itemName.equals(mTangoSpaceMenuStrings[0])) {
            // Export the ADF into application package folder and update the Listview.
            exportAdfBags(mOriginalAdfDataList.get(index).uuid);
        } else if (itemName.equals(mTangoSpaceMenuStrings[1])) {
            // Delete the ADF from Tango space and update the Tango ADF Listview.
            deleteAdfSession(mOriginalAdfDataList.get(index).uuid);
        } else if (itemName.equals(mAppSpaceMenuStrings[0])) {
            // Delete an ADF from App space and update the App space ADF Listview.
            deleteExportedBagData(mExportedAdfDataList.get(index).uuid);
        }

        updateList();
        return true;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        // Check which request we're responding to
        if (requestCode == TANGO_INTENT_ACTIVITY_CODE) {
            // Make sure the request was successful
            if (resultCode == RESULT_CANCELED) {
                Toast.makeText(this, R.string.no_permissions, Toast.LENGTH_LONG).show();
            }
        }
        updateList();
    }

    private void exportAdfBags(String sessionUuid) {
        String sessionPath = mTangoBagOutputDir + File.separator + sessionUuid;
        Log.i(TAG, "Exporting bags under " + sessionPath);
        TangoJniNative.exportBagToRawFiles(sessionPath);
    }

    private void deleteAdfSession(String uuid) {
        // delete the adf data session
    }

    private void deleteExportedBagData(String uuid) {
//        File file = new File(mTangoBagOutputDir + File.separator + uuid);
//        file.delete();
    }

    private void updateAdfSessionList() {
        File file = new File(mTangoBagOutputDir);
        File[] adfFileList = file.listFiles();
        mOriginalAdfDataList.clear();
        mExportedAdfDataList.clear();
        for (int i = 0; i < adfFileList.length; ++i) {
            String sessionUuid = adfFileList[i].getName();
            if (hasBags(sessionUuid)) {
                if (isExported(sessionUuid)) {
                    mExportedAdfDataList.add(new AdfData(sessionUuid, ""));
                } else {
                    mOriginalAdfDataList.add(new AdfData(sessionUuid, ""));
                }
            }
        }
    }

    private void updateList() {
        updateAdfSessionList();
        mOriginalAdfListAdapter.setAdfData(mOriginalAdfDataList);
        mOriginalAdfListAdapter.notifyDataSetChanged();

        mExportedAdfListAdapter.setAdfData(mExportedAdfDataList);
        mExportedAdfListAdapter.notifyDataSetChanged();
    }

    private boolean hasBags(String sessionUuid) {
        String bagPath = mTangoBagOutputDir + File.separator + sessionUuid + File.separator + "bag";
        File bagDir = new File(bagPath);
        File bagFile = new File(bagPath + File.separator + "0001.bag");
        return bagDir.exists() && bagFile.exists();
    }

    private boolean isExported(String sessionUuid) {
        String exportPath = mTangoBagOutputDir + File.separator + sessionUuid + File.separator + "export";
        File accelCsv = new File(exportPath + File.separator + "accel1" + File.separator + "data.csv");
        File imageDir = new File(exportPath + File.separator + "cam1");
        File calibrationFile = new File(exportPath + File.separator + "calibration.yaml");
        return accelCsv.exists() && imageDir.exists() && calibrationFile.exists();
    }
}

