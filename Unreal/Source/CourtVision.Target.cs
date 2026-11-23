using UnrealBuildTool;
public class CourtVisionTarget : TargetRules {
 public CourtVisionTarget(TargetInfo Target) : base(Target) {
  Type = TargetType.Game;
  DefaultBuildSettings = BuildSettingsVersion.V7;
  IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
  ExtraModuleNames.Add("CourtVision");
 }
}
