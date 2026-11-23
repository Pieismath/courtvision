using UnrealBuildTool;
public class CourtVisionEditorTarget : TargetRules {
 public CourtVisionEditorTarget(TargetInfo Target) : base(Target) {
  Type = TargetType.Editor;
  DefaultBuildSettings = BuildSettingsVersion.V7;
  IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
  ExtraModuleNames.Add("CourtVision");
 }
}
