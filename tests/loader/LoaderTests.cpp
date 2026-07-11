/**
 * @file LoaderTests.cpp
 * @brief loader 模块测试。
 */

#include "../TarFixture.hpp"
#include "../TestSupport.hpp"
#include "../ZipFixture.hpp"
#include "cc/inventory/InventoryEngine.hpp"
#include "cc/loader/PathGuard.hpp"
#include "cc/loader/ProjectLoader.hpp"
#include "cc/util/FileUtil.hpp"

#include <algorithm>
#include <string_view>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#endif

void runLoaderTests() {
    const auto hasDeferred = [](const cc::ProjectContext& context,
                                const std::filesystem::path& path, std::string_view reason) {
        return std::any_of(context.deferredFiles.begin(), context.deferredFiles.end(),
                           [&](const cc::DeferredInputFile& file) {
                               return file.relativePath == path && file.reason == reason;
                           });
    };
    const auto hasWarning = [](const cc::ProjectContext& context, std::string_view text) {
        return std::any_of(
            context.warnings.begin(), context.warnings.end(),
            [&](const std::string& warning) { return warning.find(text) != std::string::npos; });
    };

    requireTrue(cc::ImportLimits{}.maxFileCount >= 50'000U,
                "the default manifest budget should accommodate normal large repositories");
    requireTrue(cc::PathGuard::isSafeArchiveEntry("project/file.txt"),
                "safe archive entry rejected");
    requireTrue(cc::PathGuard::isSafeArchiveEntry("project/"),
                "safe archive directory entry rejected");
    requireTrue(!cc::PathGuard::isSafeArchiveEntry("../evil.txt"),
                "path traversal archive entry accepted");
    requireTrue(!cc::PathGuard::isSafeArchiveEntry("project//file.txt"),
                "empty archive path segment accepted");
    auto loaded = cc::ProjectLoader{}.load(sourceDir() / "examples/business_bad_case");
    requireTrue(loaded.ok(), "ProjectLoader should load example directory");
    requireTrue(!loaded.value().inputRoot.empty(), "ProjectLoader should create workspace input");
    requireTrue(cc::PathGuard::isInsideRoot(loaded.value().workspaceRoot, loaded.value().inputRoot),
                "workspace input should stay inside workspace");
    requireTrue(!loaded.value().inputFiles.empty(), "ProjectLoader should record input files");
    requireTrue(loaded.value().unpackStatus == "DIRECTORY_COPIED_TO_WORKSPACE",
                "directory import status should be explicit");

    auto loadedFile = cc::ProjectLoader{}.load(sourceDir() / "README.md");
    requireTrue(loadedFile.ok(), "ProjectLoader should load one material file");
    requireTrue(loadedFile.value().unpackStatus == "SINGLE_FILE_COPIED_TO_WORKSPACE",
                "single file import status should be explicit");
    requireTrue(loadedFile.value().inputFiles.size() == 1U,
                "single file import should record exactly one input");
    requireTrue(std::filesystem::is_regular_file(loadedFile.value().inputRoot / "README.md"),
                "single material should be copied into workspace input");

    const auto goodZip = contest_test::writeStoredZipFixture(
        std::filesystem::temp_directory_path() / "contest_loader_good.zip",
        {{"project/readme.txt", "zip input"}});
    auto loadedZip = cc::ProjectLoader{}.load(goodZip);
    requireTrue(loadedZip.ok(), "ProjectLoader should load zip without external unzip");
    requireTrue(loadedZip.value().archiveInput, "zip import should be marked as archive input");
    requireTrue(cc::util::readFileLimited(loadedZip.value().inputRoot / "project/readme.txt",
                                          1024U) == "zip input",
                "zip entry should be extracted into workspace input");

    const auto unsupportedRar =
        std::filesystem::temp_directory_path() / "contest_loader_unsupported.rar";
    requireTrue(cc::util::writeTextFile(unsupportedRar, "recognized container extension").ok(),
                "unsupported archive fixture should write");
    auto deferredRar = cc::ProjectLoader{}.load(unsupportedRar);
    requireTrue(deferredRar.ok() && deferredRar.value().archiveInput &&
                    deferredRar.value().unpackStatus == "ARCHIVE_METADATA_ONLY" &&
                    hasDeferred(deferredRar.value(), unsupportedRar.filename(),
                                "UNSUPPORTED_ARCHIVE_FORMAT"),
                "a recognized but unsupported archive should remain usable as metadata");

    cc::ImportLimits archiveContainerLimits;
    archiveContainerLimits.maxArchiveBytes = 1U;
    auto deferredLargeZip = cc::ProjectLoader{archiveContainerLimits}.load(goodZip);
    requireTrue(
        deferredLargeZip.ok() && deferredLargeZip.value().archiveInput &&
            deferredLargeZip.value().unpackStatus == "ARCHIVE_METADATA_ONLY" &&
            hasDeferred(deferredLargeZip.value(), goodZip.filename(),
                        "ARCHIVE_TOO_LARGE_FOR_INDEXING") &&
            !std::filesystem::exists(deferredLargeZip.value().inputRoot / "project/readme.txt"),
        "an archive beyond the container budget should not be expanded or rejected");

    const auto corruptZip =
        std::filesystem::temp_directory_path() / "contest_loader_corrupt_supported.zip";
    requireTrue(cc::util::writeTextFile(corruptZip, "this is not a zip archive").ok(),
                "corrupt zip fixture should write");
    requireTrue(!cc::ProjectLoader{}.load(corruptZip).ok(),
                "a corrupt archive in a supported format must remain a hard import failure");

    const auto mixedZip = contest_test::writeStoredZipFixture(
        std::filesystem::temp_directory_path() / "contest_loader_mixed_limits.zip",
        {{"project/readme.txt", "usable\n"},
         {"project/assets/unrecognized.payload", std::string(128U, '\0')}});
    cc::ImportLimits archiveLimits;
    archiveLimits.maxSingleFileBytes = 32U;
    archiveLimits.maxTotalBytes = 1024U;
    archiveLimits.maxExpandedBytes = 1024U;
    auto mixedZipLoaded = cc::ProjectLoader{archiveLimits}.load(mixedZip);
    requireTrue(mixedZipLoaded.ok(),
                "an oversized archive entry must not abort importing usable entries");
    requireTrue(cc::util::readFileLimited(mixedZipLoaded.value().inputRoot / "project/readme.txt",
                                          1024U) == "usable\n",
                "a normal archive entry should still be extracted beside a deferred entry");
    requireTrue(!std::filesystem::exists(mixedZipLoaded.value().inputRoot /
                                         "project/assets/unrecognized.payload"),
                "an oversized archive entry should not be expanded into the workspace");
    requireTrue(mixedZipLoaded.value().deferredFiles.size() == 1U &&
                    mixedZipLoaded.value().deferredFiles.front().relativePath ==
                        "project/assets/unrecognized.payload" &&
                    mixedZipLoaded.value().deferredFiles.front().sizeBytes == 128U,
                "an oversized entry of any type should be retained as deferred metadata");
    auto mixedZipInventory = cc::InventoryEngine{}.build(mixedZipLoaded.value());
    requireTrue(mixedZipInventory.ok(),
                "inventory should build when an archive contains deferred entries");
    const auto deferredArchiveAsset =
        std::find_if(mixedZipInventory.value().assets.begin(),
                     mixedZipInventory.value().assets.end(), [](const cc::ProjectAsset& asset) {
                         return asset.relativePath == "project/assets/unrecognized.payload";
                     });
    requireTrue(deferredArchiveAsset != mixedZipInventory.value().assets.end() &&
                    deferredArchiveAsset->sizeBytes == 128U && !deferredArchiveAsset->auditable &&
                    std::find(deferredArchiveAsset->riskFlags.begin(),
                              deferredArchiveAsset->riskFlags.end(),
                              "CONTENT_DEFERRED") != deferredArchiveAsset->riskFlags.end(),
                "a deferred archive entry should remain visible and explicitly non-auditable");

    const auto budgetZip = contest_test::writeStoredZipFixture(
        std::filesystem::temp_directory_path() / "contest_loader_copy_budget.zip",
        {{"first.txt", std::string(16U, 'a')}, {"second.data", std::string(24U, 'b')}});
    cc::ImportLimits copyBudgetLimits;
    copyBudgetLimits.maxSingleFileBytes = 64U;
    copyBudgetLimits.maxTotalBytes = 20U;
    copyBudgetLimits.maxExpandedBytes = 20U;
    auto budgetZipLoaded = cc::ProjectLoader{copyBudgetLimits}.load(budgetZip);
    requireTrue(budgetZipLoaded.ok(),
                "exhausting the archive copy budget should degrade instead of aborting import");
    requireTrue(std::filesystem::is_regular_file(budgetZipLoaded.value().inputRoot / "first.txt"),
                "entries within the remaining copy budget should be extracted");
    requireTrue(budgetZipLoaded.value().deferredFiles.size() == 1U &&
                    budgetZipLoaded.value().deferredFiles.front().relativePath == "second.data" &&
                    budgetZipLoaded.value().deferredFiles.front().reason == "COPY_BUDGET_DEFERRED",
                "the entry beyond the total copy budget should remain as deferred metadata");

    const auto badZip = contest_test::writeStoredZipFixture(std::filesystem::temp_directory_path() /
                                                                "contest_loader_bad.zip",
                                                            {{"../evil.txt", "bad"}});
    auto rejected = cc::ProjectLoader{}.load(badZip);
    requireTrue(!rejected.ok(), "ProjectLoader should reject zip-slip entries");

    const auto goodTar = contest_test::writeTarFixture(std::filesystem::temp_directory_path() /
                                                           "contest_loader_good.tar",
                                                       {{"project/readme.txt", "tar input"}});
    auto loadedTar = cc::ProjectLoader{}.load(goodTar);
    requireTrue(loadedTar.ok(), "ProjectLoader should load tar through libarchive");
    requireTrue(loadedTar.value().archiveInput, "tar import should be marked as archive input");
    requireTrue(loadedTar.value().unpackStatus == "ARCHIVE_EXTRACTED",
                "tar import status should be explicit");
    requireTrue(cc::util::readFileLimited(loadedTar.value().inputRoot / "project/readme.txt",
                                          1024U) == "tar input",
                "tar entry should be extracted into workspace input");

    const auto mixedTar = contest_test::writeTarFixture(
        std::filesystem::temp_directory_path() / "contest_loader_mixed_limits.tar",
        {{"project/readme.txt", "usable tar\n"},
         {"project/assets/unrecognized.opaque", std::string(96U, '\1')}});
    auto mixedTarLoaded = cc::ProjectLoader{archiveLimits}.load(mixedTar);
    requireTrue(mixedTarLoaded.ok(),
                "libarchive imports should also defer oversized entries without aborting");
    requireTrue(cc::util::readFileLimited(mixedTarLoaded.value().inputRoot / "project/readme.txt",
                                          1024U) == "usable tar\n" &&
                    mixedTarLoaded.value().deferredFiles.size() == 1U &&
                    mixedTarLoaded.value().deferredFiles.front().relativePath ==
                        "project/assets/unrecognized.opaque",
                "tar imports should extract usable entries and retain oversized metadata");
    auto mixedTarInventory = cc::InventoryEngine{}.build(mixedTarLoaded.value());
    requireTrue(mixedTarInventory.ok(), "inventory should build for deferred libarchive entries");
    const auto deferredTarAsset =
        std::find_if(mixedTarInventory.value().assets.begin(),
                     mixedTarInventory.value().assets.end(), [](const cc::ProjectAsset& asset) {
                         return asset.relativePath == "project/assets/unrecognized.opaque";
                     });
    requireTrue(deferredTarAsset != mixedTarInventory.value().assets.end() &&
                    deferredTarAsset->sizeBytes == 96U && !deferredTarAsset->auditable,
                "a deferred tar entry should remain visible and non-auditable");

    const auto badTar = contest_test::writeTarFixture(std::filesystem::temp_directory_path() /
                                                          "contest_loader_bad.tar",
                                                      {{"../evil.txt", "bad"}});
    auto rejectedTar = cc::ProjectLoader{}.load(badTar);
    requireTrue(!rejectedTar.ok(), "ProjectLoader should reject tar path traversal entries");

    const auto largeProject = std::filesystem::temp_directory_path() / "contest_loader_large_asset";
    std::error_code cleanupError;
    std::filesystem::remove_all(largeProject, cleanupError);
    std::filesystem::create_directories(largeProject / "admin/src/glb", cleanupError);
    requireTrue(!cleanupError, "large asset fixture should initialize");
    requireTrue(cc::util::writeTextFile(largeProject / "README.md", "ok\n").ok(),
                "large asset text fixture should write");
    requireTrue(cc::util::writeTextFile(largeProject / "admin/src/glb/base_basic_pbr2.glb",
                                        std::string(128U, '\x7f'))
                    .ok(),
                "large GLB fixture should write");
    cc::ImportLimits tightLimits;
    tightLimits.maxSingleFileBytes = 32U;
    tightLimits.maxTotalBytes = 1024U;
    auto largeLoaded = cc::ProjectLoader{tightLimits}.load(largeProject);
    requireTrue(largeLoaded.ok(),
                "one oversized binary asset must not abort importing the whole project");
    requireTrue(largeLoaded.value().deferredFiles.size() == 1U,
                "oversized asset should be recorded as deferred metadata");
    requireTrue(std::filesystem::is_regular_file(largeLoaded.value().inputRoot / "README.md"),
                "normal project files should still enter the isolated copy");
    auto largeInventory = cc::InventoryEngine{}.build(largeLoaded.value());
    requireTrue(largeInventory.ok(), "inventory should include deferred project assets");
    const auto deferredModel =
        std::find_if(largeInventory.value().assets.begin(), largeInventory.value().assets.end(),
                     [](const cc::ProjectAsset& asset) {
                         return asset.relativePath == "admin/src/glb/base_basic_pbr2.glb";
                     });
    requireTrue(deferredModel != largeInventory.value().assets.end() &&
                    deferredModel->role == cc::AssetRole::ModelArtifact &&
                    !deferredModel->auditable,
                "deferred GLB should remain visible as a metadata-only model asset");
    std::filesystem::remove_all(largeProject, cleanupError);

    const auto genericProject =
        std::filesystem::temp_directory_path() / "contest_loader_generic_degradation";
    std::filesystem::remove_all(genericProject, cleanupError);
    std::filesystem::create_directories(genericProject / "one/two/three", cleanupError);
    requireTrue(!cleanupError, "generic degradation fixture should initialize");
    requireTrue(
        cc::util::writeTextFile(genericProject / "README.md", "usable\n").ok() &&
            cc::util::writeTextFile(genericProject / "oversized.csv", std::string(96U, 'x')).ok() &&
            cc::util::writeTextFile(genericProject / "one/two/three/settings.yaml",
                                    "enabled: true\n")
                .ok(),
        "generic degradation files should write");
    cc::ImportLimits genericLimits;
    genericLimits.maxSingleFileBytes = 32U;
    genericLimits.maxTotalBytes = 1024U;
    genericLimits.maxPathDepth = 3U;
    auto genericLoaded = cc::ProjectLoader{genericLimits}.load(genericProject);
    requireTrue(genericLoaded.ok() &&
                    std::filesystem::is_regular_file(genericLoaded.value().inputRoot / "README.md"),
                "usable files should still import beside independently deferred files");
    requireTrue(hasDeferred(genericLoaded.value(), "oversized.csv", "LARGE_BINARY_DEFERRED"),
                "the size limit should apply to every file type, including text tables");
    requireTrue(
        hasDeferred(genericLoaded.value(), "one/two/three/settings.yaml", "PATH_DEPTH_LIMIT"),
        "an overly deep file should remain in the manifest as metadata");
    requireTrue(!std::filesystem::exists(genericLoaded.value().inputRoot / "oversized.csv") &&
                    !std::filesystem::exists(genericLoaded.value().inputRoot /
                                             "one/two/three/settings.yaml"),
                "deferred files must not be partially copied");
    std::filesystem::remove_all(genericProject, cleanupError);

    const auto specialProject =
        std::filesystem::temp_directory_path() / "contest_loader_special_entries";
    const auto outsideFile =
        std::filesystem::temp_directory_path() / "contest_loader_outside_target.txt";
    std::filesystem::remove_all(specialProject, cleanupError);
    std::filesystem::remove(outsideFile, cleanupError);
    std::filesystem::create_directories(specialProject, cleanupError);
    requireTrue(!cleanupError && cc::util::writeTextFile(outsideFile, "must not be copied").ok() &&
                    cc::util::writeTextFile(specialProject / "normal.json", "{}\n").ok(),
                "special entry fixture should initialize");
    std::filesystem::create_symlink(outsideFile, specialProject / "external-link.txt",
                                    cleanupError);
    requireTrue(!cleanupError, "directory symlink fixture should initialize");
#if defined(__unix__) || defined(__APPLE__)
    requireTrue(::mkfifo((specialProject / "events.pipe").c_str(), 0600) == 0,
                "FIFO fixture should initialize");
#endif
    auto specialLoaded = cc::ProjectLoader{}.load(specialProject);
    requireTrue(specialLoaded.ok() && std::filesystem::is_regular_file(
                                          specialLoaded.value().inputRoot / "normal.json"),
                "links and special files must not abort copying ordinary siblings");
    requireTrue(hasDeferred(specialLoaded.value(), "external-link.txt", "SYMLINK_DEFERRED") &&
                    !std::filesystem::exists(specialLoaded.value().inputRoot / "external-link.txt"),
                "directory symlinks should be recorded without following their target");
#if defined(__unix__) || defined(__APPLE__)
    requireTrue(hasDeferred(specialLoaded.value(), "events.pipe", "FIFO_DEFERRED") &&
                    !std::filesystem::exists(specialLoaded.value().inputRoot / "events.pipe"),
                "FIFO entries should be recorded without ever being opened");
#endif

    const auto rootLink =
        std::filesystem::temp_directory_path() / "contest_loader_selected_link.txt";
    std::filesystem::remove(rootLink, cleanupError);
    std::filesystem::create_symlink(outsideFile, rootLink, cleanupError);
    requireTrue(!cleanupError, "selected symlink fixture should initialize");
    auto selectedLink = cc::ProjectLoader{}.load(rootLink);
    requireTrue(selectedLink.ok() && selectedLink.value().unpackStatus == "INPUT_METADATA_ONLY" &&
                    hasDeferred(selectedLink.value(), rootLink.filename(), "SYMLINK_DEFERRED"),
                "a selected symlink should remain metadata-only instead of following its target");
    auto selectedLinkInventory = cc::InventoryEngine{}.build(selectedLink.value());
    requireTrue(selectedLinkInventory.ok() && selectedLinkInventory.value().assets.size() == 1U &&
                    selectedLinkInventory.value().assets.front().relativePath ==
                        rootLink.filename() &&
                    !selectedLinkInventory.value().assets.front().auditable,
                "a selected special input must remain visible in the metadata inventory");
    std::filesystem::remove(rootLink, cleanupError);
    std::filesystem::remove_all(specialProject, cleanupError);
    std::filesystem::remove(outsideFile, cleanupError);

    const auto budgetProject =
        std::filesystem::temp_directory_path() / "contest_loader_directory_budget";
    std::filesystem::remove_all(budgetProject, cleanupError);
    std::filesystem::create_directories(budgetProject, cleanupError);
    requireTrue(!cleanupError &&
                    cc::util::writeTextFile(budgetProject / "first.txt", "12345678").ok() &&
                    cc::util::writeTextFile(budgetProject / "second.bin", "abcdefgh").ok(),
                "directory copy budget fixture should initialize");
    cc::ImportLimits directoryBudgetLimits;
    directoryBudgetLimits.maxSingleFileBytes = 32U;
    directoryBudgetLimits.maxTotalBytes = 10U;
    auto budgetLoaded = cc::ProjectLoader{directoryBudgetLimits}.load(budgetProject);
    requireTrue(budgetLoaded.ok(),
                "a directory should remain importable after its copy budget is exhausted");
    const auto copiedBudgetFiles =
        static_cast<int>(
            std::filesystem::is_regular_file(budgetLoaded.value().inputRoot / "first.txt")) +
        static_cast<int>(
            std::filesystem::is_regular_file(budgetLoaded.value().inputRoot / "second.bin"));
    requireTrue(
        copiedBudgetFiles == 1 &&
            std::filesystem::is_regular_file(budgetLoaded.value().inputRoot / "first.txt") &&
            hasDeferred(budgetLoaded.value(), "second.bin", "COPY_BUDGET_DEFERRED") &&
            std::count_if(budgetLoaded.value().deferredFiles.begin(),
                          budgetLoaded.value().deferredFiles.end(),
                          [](const cc::DeferredInputFile& file) {
                              return file.reason == "COPY_BUDGET_DEFERRED";
                          }) == 1,
        "exhausting a directory copy budget should defer only the remaining file");
    std::filesystem::remove_all(budgetProject, cleanupError);

    const auto manifestProject =
        std::filesystem::temp_directory_path() / "contest_loader_manifest_budget";
    std::filesystem::remove_all(manifestProject, cleanupError);
    std::filesystem::create_directories(manifestProject, cleanupError);
    requireTrue(!cleanupError, "manifest budget fixture should initialize");
    for (int index = 0; index < 4; ++index) {
        requireTrue(cc::util::writeTextFile(
                        manifestProject / ("entry-" + std::to_string(index) + ".txt"), "x")
                        .ok(),
                    "manifest fixture file should write");
    }
    cc::ImportLimits manifestLimits;
    manifestLimits.maxFileCount = 2U;
    auto manifestLoaded = cc::ProjectLoader{manifestLimits}.load(manifestProject);
    requireTrue(manifestLoaded.ok() && manifestLoaded.value().inputFiles.size() == 2U &&
                    manifestLoaded.value().inputFiles[0] == "entry-0.txt" &&
                    manifestLoaded.value().inputFiles[1] == "entry-1.txt" &&
                    hasWarning(manifestLoaded.value(), "其余条目未扫描或复制"),
                "a custom manifest cap should be deterministic, bounded and explicitly reported");
    std::filesystem::remove_all(manifestProject, cleanupError);

    const auto unreadableFile =
        std::filesystem::temp_directory_path() / "contest_loader_unreadable.txt";
    requireTrue(cc::util::writeTextFile(unreadableFile, "permission test").ok(),
                "unreadable single-file fixture should write");
    std::filesystem::permissions(unreadableFile, std::filesystem::perms::none,
                                 std::filesystem::perm_options::replace, cleanupError);
    requireTrue(!cleanupError, "unreadable fixture permissions should update");
    auto unreadableLoaded = cc::ProjectLoader{}.load(unreadableFile);
    std::filesystem::permissions(unreadableFile, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace, cleanupError);
    requireTrue(unreadableLoaded.ok() &&
                    hasDeferred(unreadableLoaded.value(), unreadableFile.filename(), "COPY_FAILED"),
                "a single regular file copy failure should degrade to metadata-only");
    std::filesystem::remove(unreadableFile, cleanupError);

    requireTrue(!cc::ProjectLoader{}
                     .load(std::filesystem::temp_directory_path() /
                           "contest_loader_path_that_does_not_exist")
                     .ok(),
                "a missing root path must remain a hard failure");
    std::filesystem::remove(unsupportedRar, cleanupError);
    std::filesystem::remove(corruptZip, cleanupError);
}
