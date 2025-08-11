# 3FS Permission System Documentation

## Overview

3FS implements a two-layer permission system that combines cluster-level authentication with POSIX-style access control lists (ACLs). This document covers the complete permission architecture, troubleshooting, and configuration.

## Architecture Components

### 1. Cluster Authentication Layer

**Token-Based Authentication**
- **Purpose**: Authorizes FUSE clients to access 3FS cluster services
- **Scope**: Cluster-wide access control
- **Implementation**: String-based tokens with optional TTL
- **Location**: `src/fbs/core/user/User.h:15-23`

**Token Types:**
- **Endless Tokens**: Primary authentication, no expiration
- **TTL Tokens**: Time-limited tokens with expiration
- **Multi-Token Support**: Up to 5 tokens per user for rotation

### 2. POSIX Permission Layer

**ACL Structure** (`src/fbs/meta/Schema.h:46-68`)
```cpp
struct Acl {
  SERDE_STRUCT_FIELD(uid, Uid(0));        // Owner user ID
  SERDE_STRUCT_FIELD(gid, Gid(0));        // Owner group ID  
  SERDE_STRUCT_FIELD(perm, Permission(0)); // POSIX permission bits
  SERDE_STRUCT_FIELD(iflags, IFlags(0));   // Extended flags (immutable, etc.)
}
```

**Permission Checking** (`src/fbs/meta/Schema.cc:24-45`)
```cpp
Result<Void> Acl::checkPermission(const UserInfo &user, AccessType type) const {
  if (user.uid == 0) return Void{};           // Root bypasses all checks
  else if (user.uid == uid) permNeeded = type << 6;    // Owner permissions
  else if (user.inGroup(gid)) permNeeded = type << 3;  // Group permissions  
  else permNeeded = type;                              // Other permissions
  
  if ((perm & permNeeded) != permNeeded) {
    return makeError(MetaCode::kNoPermission, ...);
  }
}
```

### 3. Access Types

**Defined in** `src/fbs/meta/Common.h:56-62`
```cpp
enum AccessType {
  EXEC = 1,   // Execute/traverse permission
  WRITE = 2,  // Write/modify permission  
  READ = 4,   // Read permission
};
```

## FUSE Integration

### User Context Creation

**Pattern Used Throughout** `src/fuse/FuseOps.cc`
```cpp
auto userInfo = UserInfo(flat::Uid(fuse_req_ctx(req)->uid),    // Process UID
                        flat::Gid(fuse_req_ctx(req)->gid),    // Process GID  
                        d.fuseToken);                          // Cluster token
```

### Credential Sources

1. **uid/gid**: Come from `fuse_req_ctx(req)` - the **requesting process's credentials**
2. **token**: Loaded at FUSE daemon startup from file or environment variable
3. **groups**: Populated from process supplementary groups

### FUSE Mount Options

**Configuration** (`src/fuse/FuseMainLoop.cc:27-31`)
```cpp
if (allowOther) {
  fuseArgs.push_back("allow_other");      // Allow non-mounting users
  fuseArgs.push_back("default_permissions"); // Enable kernel permission checks
}
```

**Key Settings:**
- `allow_other = true` (default): Enables multi-user access
- `default_permissions`: Kernel performs basic permission validation
- `max_uid = 1_M`: Maximum supported UID for user configurations

## Token Management

### Loading Mechanism

**Priority Order** (`src/fuse/FuseClients.cc:68-75`):
1. **Environment Variable**: `HF3FS_FUSE_TOKEN` (highest priority)
2. **Configuration File**: `token_file` parameter

```cpp
if (const char *env_p = std::getenv("HF3FS_FUSE_TOKEN")) {
  fuseToken = std::string(env_p);
} else {
  auto tokenRes = loadFile(tokenFile);
  fuseToken = folly::trimWhitespace(*tokenRes);
}
```

### Token Configuration

**In Launcher Config** (`src/fuse/FuseLauncherConfig.h:16`)
```toml
token_file = "/path/to/token/file"
```

**In FUSE Config** (`src/fuse/FuseConfig.h:16`)
```toml  
token_file = "/path/to/token/file"
```

## Permission Flow Examples

### Directory Creation (mkdir)

**Code Path**: `src/meta/store/ops/Mkdirs.cc:56`
```cpp
CO_RETURN_ON_ERROR(parent->acl.checkPermission(req_.user, AccessType::WRITE));
```

**Flow**:
1. User process calls `mkdir("/path/to/new/dir")`
2. FUSE extracts process credentials: `{uid: 1000, gid: 1000}`
3. Creates UserInfo: `{uid: 1000, gid: 1000, token: "cluster_token"}`
4. Resolves parent directory ACL
5. Checks if user 1000 has WRITE permission on parent
6. Permission calculation:
   - If uid=0: Always allowed
   - If uid=owner: Check owner write bit (0200)
   - If gid=group: Check group write bit (0020)  
   - Else: Check other write bit (0002)

### File Access (read/write)

**Similar pattern in all operations**:
- Extract process credentials from FUSE context
- Load file/directory ACL from metadata
- Check appropriate permission (READ/WRITE/EXEC)
- Perform operation or return permission error

## SystemD Service Configuration

### Default Configuration

**Service Files**: 
- `deploy/systemd/hf3fs_fuse_main.service`
- `testing_configs/hf3fs_fuse_main.service`

```ini
[Service]
LimitNOFILE=1000000
ExecStart=/opt/3fs/bin/hf3fs_fuse_main --launcher_cfg /opt/3fs/etc/hf3fs_fuse_main_launcher.toml
Type=simple
# No User= directive means runs as root (uid=0)
```

### Permission Implications

**Running as SystemD Service (Root)**:
- **FUSE daemon**: Runs as root (uid=0)
- **Mount operations**: Performed with root privileges
- **User operations**: Still use requesting process credentials
- **Permission bypass**: Root operations bypass ACL checks

**Running as Specific User**:
```ini
[Service]
User=username
Group=groupname
# Service runs with specified user credentials
```

## Troubleshooting Guide

### Common Permission Errors

#### 1. "No Permission" Error in Mkdirs

**Error Location**: `src/meta/store/ops/Mkdirs.cc:56`
**Cause**: User lacks WRITE permission on parent directory

**Solutions**:
1. **Run FUSE as root via systemd** (recommended)
2. **Fix parent directory permissions** 
3. **Add user to appropriate group**
4. **Ensure proper directory hierarchy setup**

#### 2. Token Authentication Failures

**Symptoms**: Connection refused, authentication errors
**Causes**: 
- Missing or invalid token file
- Incorrect token content
- Token expiration (for TTL tokens)

**Solutions**:
1. Verify token file exists and is readable
2. Check token content matches cluster configuration
3. Use environment variable for testing: `export HF3FS_FUSE_TOKEN="your_token"`
4. Check token expiration with admin tools

#### 3. Multi-User Access Issues

**Symptoms**: Operations work for mounting user but fail for others
**Causes**:
- `allow_other = false` in configuration
- Missing `default_permissions` FUSE option
- Restrictive parent directory permissions

**Solutions**:
1. Set `allow_other = true` in FUSE configuration
2. Ensure FUSE mounts with `allow_other` and `default_permissions`
3. Configure parent directories with appropriate permissions (755/775)

### Debugging Commands

#### Check Process Credentials
```bash
id                              # Current user credentials
ps aux | grep hf3fs_fuse_main  # FUSE daemon process owner
```

#### Test Token Authentication
```bash
export HF3FS_FUSE_TOKEN="test_token"
./hf3fs_fuse_main --launcher_cfg config.toml
```

#### Verify FUSE Mount Options
```bash
mount | grep 3fs               # Check active mount options
fusermount3 -u /mount/point    # Unmount for testing
```

#### Check SystemD Service Status
```bash
systemctl status hf3fs_fuse_main
journalctl -u hf3fs_fuse_main -f  # Follow logs
```

## Best Practices

### Security Recommendations

1. **Token Security**:
   - Store tokens in protected files (600 permissions)
   - Use environment variables for testing only
   - Rotate tokens regularly using admin tools
   - Never commit tokens to source control

2. **Service Configuration**:
   - Run FUSE daemon as root via systemd for maximum compatibility
   - Use specific users only when required by security policy
   - Configure appropriate file limits (`LimitNOFILE`)

3. **Directory Permissions**:
   - Set parent directories to 755 (rwxr-xr-x) for basic access
   - Use 775 (rwxrwxr-x) for group collaboration
   - Avoid 777 unless absolutely necessary

### Operational Guidelines

1. **Initial Setup**:
   - Configure tokens before starting services
   - Test authentication with admin CLI tools
   - Verify mount points and permissions

2. **Multi-User Environments**:
   - Enable `allow_other = true`
   - Ensure consistent group membership
   - Document permission model for users

3. **Troubleshooting Workflow**:
   - Check token authentication first
   - Verify process credentials vs ACL permissions
   - Test with root access to isolate permission issues
   - Use logging to trace permission checks

## Configuration Reference

### Key Configuration Files

- **Launcher Config**: `hf3fs_fuse_main_launcher.toml`
  - `token_file`: Path to authentication token
  - `allow_other`: Enable multi-user access
  - `mountpoint`: FUSE mount location

- **FUSE Config**: `hf3fs_fuse_main.toml`  
  - Permission-related timeouts and caching
  - User configuration limits (`max_uid`)
  - Read/write behavior settings

- **SystemD Service**: `hf3fs_fuse_main.service`
  - Service user/group configuration
  - Resource limits and startup parameters

### Environment Variables

- `HF3FS_FUSE_TOKEN`: Override token file with direct token value
- Standard systemd environment variables for service configuration

This documentation provides a comprehensive reference for understanding, configuring, and troubleshooting the 3FS permission system.