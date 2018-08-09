package app_update_service;

public class ServiceDiff {
	
	private native static int diff(String oldFile, String newFile, String patchFile);
	
	public static void main(String[] args) {
		int result = diff("F:\\java-project\\app_update_service\\src\\weixin_v6.0.apk", 
				"F:\\java-project\\app_update_service\\src\\weixin_1080.apk", 
				"F:\\java-project\\app_update_service\\src\\weixin.patch");
		System.out.print(result);
	}
	
	static {
		//System.load("F:\\java-project\\app_update_service\\src\\bsdiff.dll");
		System.loadLibrary("bsdiff");
	}
}
