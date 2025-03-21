#pragma once

#include "clickhouse_config.h"

#include <string>
#include <unordered_set>
#include <vector>
#include <memory>

#include <Common/Logger.h>

#include <DBPoco/DOM/Document.h>
#include <DBPoco/DOM/DOMParser.h>
#include <DBPoco/DOM/DOMWriter.h>
#include <DBPoco/DOM/NodeList.h>
#include <DBPoco/DOM/NamedNodeMap.h>
#include <DBPoco/AutoPtr.h>
#include <DBPoco/DirectoryIterator.h>
#include <DBPoco/ConsoleChannel.h>
#include <DBPoco/Util/AbstractConfiguration.h>


namespace DBPoco { class Logger; }

namespace zkutil
{
    class ZooKeeperNodeCache;
    using EventPtr = std::shared_ptr<DBPoco::Event>;
}

namespace DB
{

using ConfigurationPtr = DBPoco::AutoPtr<DBPoco::Util::AbstractConfiguration>;
using XMLDocumentPtr = DBPoco::AutoPtr<DBPoco::XML::Document>;

class ConfigProcessor
{
public:
    using Substitutions = std::vector<std::pair<std::string, std::string>>;

    /// Set log_to_console to true if the logging subsystem is not initialized yet.
    explicit ConfigProcessor(
        const std::string & path,
        bool throw_on_bad_incl = false,
        bool log_to_console = false,
        const Substitutions & substitutions = Substitutions());

    /// Perform config includes and substitutions and return the resulting XML-document.
    ///
    /// Suppose path is "/path/file.xml"
    /// 1) Merge XML trees of /path/file.xml with XML trees of all files from /path/{conf,file}.d/*.{conf,xml}
    ///    * If an element has a "replace" attribute, replace the matching element with it.
    ///    * If an element has a "remove" attribute, remove the matching element.
    ///    * Else, recursively merge child elements.
    /// 2) Determine the includes file from the config: <include_from>/path2/metrika.xml</include_from>
    ///    If this path is not configured, use /etc/metrika.xml
    /// 3) Replace elements matching the "<foo incl="bar"/>" pattern with
    ///    "<foo>contents of the clickhouse/bar element in metrika.xml</foo>"
    /// 4) If zk_node_cache is non-NULL, replace elements matching the "<foo from_zk="/bar">" pattern with
    ///    "<foo>contents of the /bar ZooKeeper node</foo>".
    ///    If has_zk_includes is non-NULL and there are such elements, set has_zk_includes to true.
    XMLDocumentPtr processConfig(
        bool * has_zk_includes = nullptr,
        zkutil::ZooKeeperNodeCache * zk_node_cache = nullptr,
        const zkutil::EventPtr & zk_changed_event = nullptr);

    XMLDocumentPtr parseConfig(const std::string & config_path);

    /// These configurations will be used if there is no configuration file.
    static void registerEmbeddedConfig(std::string name, std::string_view content);


    /// loadConfig* functions apply processConfig and create DBPoco::Util::XMLConfiguration.
    /// The resulting XML document is saved into a file with the name
    /// resulting from adding "-preprocessed" suffix to the path file name.
    /// E.g., config.xml -> config-preprocessed.xml

    struct LoadedConfig
    {
        ConfigurationPtr configuration;
        bool has_zk_includes;
        bool loaded_from_preprocessed;
        XMLDocumentPtr preprocessed_xml;
        std::string config_path;
    };

    /// If allow_zk_includes is true, expect that the configuration XML can contain from_zk nodes.
    /// If it is the case, set has_zk_includes to true and don't write config-preprocessed.xml,
    /// expecting that config would be reloaded with zookeeper later.
    LoadedConfig loadConfig(bool allow_zk_includes = false);

    /// If fallback_to_preprocessed is true, then if KeeperException is thrown during config
    /// processing, load the configuration from the preprocessed file.
    LoadedConfig loadConfigWithZooKeeperIncludes(
        zkutil::ZooKeeperNodeCache & zk_node_cache,
        const zkutil::EventPtr & zk_changed_event,
        bool fallback_to_preprocessed = false);

    /// Save preprocessed config to specified directory.
    /// If preprocessed_dir is empty - calculate from loaded_config.path + /preprocessed_configs/
    void savePreprocessedConfig(LoadedConfig & loaded_config, std::string preprocessed_dir);

    /// Set path of main config.xml. It will be cut from all configs placed to preprocessed_configs/
    static void setConfigPath(const std::string & config_path);

    using Files = std::vector<std::string>;

    static Files getConfigMergeFiles(const std::string & config_path);

    /// Is the file named as result of config preprocessing, not as original files.
    static bool isPreprocessedFile(const std::string & config_path);

#if USE_SSL
    /// Encrypt text value
    static std::string encryptValue(const std::string & codec_name, const std::string & value);

    /// Decrypt value
    static std::string decryptValue(const std::string & codec_name, const std::string & value);
#endif

    static inline const auto SUBSTITUTION_ATTRS = {"incl", "from_zk", "from_env"};

private:
    const std::string path;
    std::string preprocessed_path;

    bool throw_on_bad_incl;

    LoggerPtr log;
    DBPoco::AutoPtr<DBPoco::Channel> channel_ptr;

    Substitutions substitutions;

    DBPoco::AutoPtr<DBPoco::XML::NamePool> name_pool;
    DBPoco::XML::DOMParser dom_parser;

    using NodePtr = DBPoco::AutoPtr<DBPoco::XML::Node>;

#if USE_SSL
    void decryptRecursive(DBPoco::XML::Node * config_root);

    /// Decrypt elements in config with specified encryption attributes
    void decryptEncryptedElements(LoadedConfig & loaded_config);
#endif

    void hideRecursive(DBPoco::XML::Node * config_root);
    XMLDocumentPtr hideElements(XMLDocumentPtr xml_tree);

    void mergeRecursive(XMLDocumentPtr config, DBPoco::XML::Node * config_root, const DBPoco::XML::Node * with_root);

    /// If config root node name is not 'clickhouse' and merging config's root node names doesn't match, bypasses merging and returns false.
    /// For compatibility root node 'yandex' considered equal to 'clickhouse'.
    bool merge(XMLDocumentPtr config, XMLDocumentPtr with);

    void doIncludesRecursive(
            XMLDocumentPtr config,
            XMLDocumentPtr include_from,
            DBPoco::XML::Node * node,
            zkutil::ZooKeeperNodeCache * zk_node_cache,
            const zkutil::EventPtr & zk_changed_event,
            std::unordered_set<std::string> & contributing_zk_paths);
};

}
